#pragma once
#include "pack.hpp"
#include "result.hpp"
#include "slab.hpp"
#include "wal.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace reddish
{

// An element value. Collections are not part of this type: a list or hash lives
// in the keyspace node itself in a packed representation, so this variant only
// ever carries a string or an integer.
using Value = std::variant<std::string, std::int64_t>;

// Non-owning view of a stored value, valid only while the database lock is held.
struct ValueView
{
    enum class Kind
    {
        string,
        integer,
        list,
        hash,
    };

    Kind kind = Kind::string;
    std::string_view text;
    std::int64_t integer = 0;
};

struct DatabaseStats
{
    std::size_t key_count;
    std::size_t approximate_memory_bytes;
    std::size_t wal_bytes;
    std::uint64_t evictions;
    std::uint64_t expired_keys;
    std::size_t reserved_memory_bytes;
};

// Keyspace with a compact on-heap representation.
//
// Every key is exactly one slab allocation: a 32-byte node header, the key
// bytes, and the payload, all inline and all sized to the request. The previous
// implementation stored an unordered_map node, a separate std::string heap block
// for the value, and a std::list node for exact-LRU bookkeeping, each carrying
// its own allocator header.
//
//   strings   payload is the value bytes, value length in the count word
//   integers  payload is an 8-byte little-endian integer
//   lists     payload is a ListBody followed by a packed element run
//   hashes    payload is a packed run of field/value element pairs
//
// Lists and hashes are packed (listpack style) so a one-element collection costs
// one small block instead of a std::deque or std::unordered_map. A hash switches
// to a real hash table once it grows past hash_packed_limit entries.
//
// The node records its slab size class rather than its byte size, and its exact
// value length lives in the count word, which is what keeps the header at 32
// bytes. Deadlines live in a side table so that only keys with a TTL pay for
// expiry bookkeeping and deadlines keep millisecond precision.
//
// Intrusive exact LRU links live inside each node. Access and eviction are O(1)
// and need no separate list allocation, random sampling or wrapping timestamp.
//
// Locking contract:
//   * Data path methods (set, get, value_view, increment, push_*, pop_*,
//     list_length, hash_*, expire, ttl, del, stats, key_count, sequence) do NOT
//     lock. The caller must hold an exclusive lock on mutex() across the batch so
//     that returned views stay valid; single threaded callers may skip it.
//   * Maintenance and WAL-open methods acquire the lock themselves.
//   * Reads hide expired keys; the maintenance thread removes them physically.
class Database
{
  public:
    explicit Database(std::size_t max_keys = 1024);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Longest key the packed 21-bit length field can describe.
    static constexpr std::size_t max_key_length = 0x001FFFFF;

    void set(std::string_view key, std::string_view val);
    Result<std::optional<std::string>> get(std::string_view key);
    Result<std::optional<ValueView>> value_view(std::string_view key);

    Result<std::int64_t> increment(std::string_view key);

    Result<std::int64_t> push_left(std::string_view key, Value value);
    Result<std::int64_t> push_right(std::string_view key, Value value);
    // String fast paths. They take the element bytes directly so that pushing a
    // string argument never materialises a Value, which would heap-allocate the
    // element on every call.
    Result<std::int64_t> push_left_text(std::string_view key, std::string_view value);
    Result<std::int64_t> push_right_text(std::string_view key, std::string_view value);
    Result<std::optional<Value>> pop_left(std::string_view key);
    Result<std::optional<Value>> pop_right(std::string_view key);
    Result<std::int64_t> list_length(std::string_view key);

    Result<std::int64_t> hash_set(std::string_view key, std::string_view field, Value value);
    Result<std::int64_t> hash_set_text(std::string_view key, std::string_view field,
                                       std::string_view value);
    Result<std::optional<Value>> hash_get(std::string_view key, std::string_view field);
    Result<std::optional<ValueView>> hash_field(std::string_view key, std::string_view field);
    Result<bool> hash_del(std::string_view key, std::string_view field);
    Result<std::int64_t> hash_length(std::string_view key);

    bool expire(std::string_view key, std::int64_t seconds);
    std::int64_t ttl(std::string_view key);

    // Expires due keys. `limit` bounds the work so a background thread can run it
    // in short slices; the default drains every due key.
    static constexpr std::size_t no_expiry_limit = static_cast<std::size_t>(-1);
    void remove_expired(std::size_t limit = no_expiry_limit);

    // Evicts exact LRU victims until at most `target` keys remain, removing no
    // more than `limit` keys per call so a background thread can bound how long it
    // holds the lock. Returns the number of keys actually evicted.
    std::size_t evict_to(std::size_t target, std::size_t limit);

    void open_wal(const std::filesystem::path &path, WalSync sync = WalSync::everysec);
    void flush_wal();
    void begin_wal_batch()
    {
        if (wal_) wal_->begin_batch();
    }
    void end_wal_batch()
    {
        if (wal_) wal_->end_batch();
    }
    bool wal_failed() const noexcept
    {
        return wal_ && wal_->failed();
    }
    bool maintenance_needed() const noexcept
    {
        return ttl_hint_.load() || key_hint_.load() > max_keys_;
    }
    bool over_capacity() const noexcept
    {
        return key_hint_.load() > max_keys_;
    }
    DatabaseStats stats() const;
    bool del(std::string_view key);

    std::size_t key_count() const noexcept
    {
        return key_total_;
    }
    std::size_t max_keys() const noexcept
    {
        return max_keys_;
    }

    // Bumped by every mutation for diagnostics.
    std::uint64_t sequence() const noexcept
    {
        return sequence_;
    }

    std::shared_mutex &mutex() const noexcept
    {
        return mutex_;
    }

  private:
    enum class NodeType : std::uint8_t
    {
        string = 0,
        integer = 1,
        list = 2,
        hash = 3,
    };

    struct Node
    {
        Node *next;             // bucket chain
        std::uint32_t key_meta; // key length, type, flags and size class
        std::uint32_t count;    // string length, or list/hash element bookkeeping
        Node *lru_previous;
        Node *lru_next;
    };
    static_assert(sizeof(void *) != 8 || sizeof(Node) == 32);

    static constexpr std::uint32_t key_length_mask = 0x001FFFFF;
    static constexpr std::uint8_t flag_hash_promoted = 0x01;
    static constexpr std::uint8_t flag_has_ttl = 0x02;
    static constexpr std::uint32_t hash_packed_limit = 128;
    static constexpr std::uint32_t hash_packed_byte_limit = 4096;
    static constexpr std::size_t initial_bucket_count = 16;

    // Size class sentinel for nodes too large for any slab class. Such nodes are
    // served by operator new at their exact size, and the exact size is recorded
    // in a word between the key and the payload.
    static constexpr std::uint8_t oversized_class = 63;

    // The key region is padded so the payload starts on an 8-byte boundary,
    // keeping ListBody and the promoted-hash pointer naturally aligned.
    static constexpr std::uint32_t key_stride(std::uint32_t key_length) noexcept
    {
        return (key_length + 7U) & ~7U;
    }

    // --- header accessors -----------------------------------------------
    static std::uint32_t make_key_meta(std::size_t key_length, NodeType type, std::uint8_t flags,
                                       std::uint8_t size_class) noexcept
    {
        return (static_cast<std::uint32_t>(key_length) & key_length_mask) |
               (static_cast<std::uint32_t>(type) << 21) |
               (static_cast<std::uint32_t>(flags) << 24) |
               (static_cast<std::uint32_t>(size_class) << 26);
    }
    static std::uint32_t node_key_length(const Node &node) noexcept
    {
        return node.key_meta & key_length_mask;
    }
    static NodeType node_type(const Node &node) noexcept
    {
        return static_cast<NodeType>((node.key_meta >> 21) & 0x07U);
    }
    static std::uint8_t node_flags(const Node &node) noexcept
    {
        return static_cast<std::uint8_t>((node.key_meta >> 24) & 0x03U);
    }
    static std::uint8_t node_size_class(const Node &node) noexcept
    {
        return static_cast<std::uint8_t>((node.key_meta >> 26) & 0x3FU);
    }
    static void set_type(Node &node, NodeType type) noexcept
    {
        node.key_meta = (node.key_meta & ~(0x07U << 21)) | (static_cast<std::uint32_t>(type) << 21);
    }
    static void set_flags(Node &node, std::uint8_t flags) noexcept
    {
        node.key_meta =
            (node.key_meta & ~(0x03U << 24)) | (static_cast<std::uint32_t>(flags & 0x03U) << 24);
    }
    static void set_size_class(Node &node, std::uint8_t size_class) noexcept
    {
        node.key_meta =
            (node.key_meta & ~(0x3FU << 26)) | (static_cast<std::uint32_t>(size_class) << 26);
    }

    static std::uint32_t node_block_size(const Node &node) noexcept
    {
        if (node_size_class(node) == oversized_class)
        {
            std::uint32_t size = 0;
            std::memcpy(&size, node_key(node) + key_stride(node_key_length(node)), sizeof(size));
            return size;
        }
        return static_cast<std::uint32_t>(SlabPool::class_size(node_size_class(node)));
    }

    // --- payload accessors ----------------------------------------------
    static char *node_key(Node &node) noexcept
    {
        return reinterpret_cast<char *>(&node) + sizeof(Node);
    }
    static const char *node_key(const Node &node) noexcept
    {
        return reinterpret_cast<const char *>(&node) + sizeof(Node);
    }
    // Oversized nodes carry their exact size word between the key and the payload.
    static std::uint32_t payload_offset(const Node &node) noexcept
    {
        return static_cast<std::uint32_t>(sizeof(Node)) + key_stride(node_key_length(node)) +
               (node_size_class(node) == oversized_class
                    ? static_cast<std::uint32_t>(sizeof(std::uint64_t))
                    : 0U);
    }
    static char *node_body(Node &node) noexcept
    {
        return reinterpret_cast<char *>(&node) + payload_offset(node);
    }
    static const char *node_body(const Node &node) noexcept
    {
        return reinterpret_cast<const char *>(&node) + payload_offset(node);
    }
    // Bytes available in the payload, derived from the allocated block size.
    static std::uint32_t node_body_capacity(const Node &node) noexcept
    {
        return node_block_size(node) - payload_offset(node);
    }

    static std::string_view string_value(const Node &node) noexcept
    {
        return {node_body(node), node.count};
    }
    static std::int64_t integer_value(const Node &node) noexcept
    {
        std::int64_t value = 0;
        std::memcpy(&value, node_body(node), sizeof(value));
        return value;
    }
    static void set_integer_value(Node &node, std::int64_t value) noexcept
    {
        std::memcpy(node_body(node), &value, sizeof(value));
    }

    // Packed hashes keep both the field count and the packed byte count in the
    // single count word: count in the low half, used bytes in the high half. Both
    // stay well below 65536 because a packed hash is promoted to a real hash
    // table once it exceeds hash_packed_limit entries or hash_packed_byte_limit
    // bytes.
    static std::uint32_t hash_count(const Node &node) noexcept
    {
        return node.count & 0xFFFFU;
    }
    static void set_hash_count(Node &node, std::uint32_t count) noexcept
    {
        node.count = (node.count & 0xFFFF0000U) | (count & 0xFFFFU);
    }
    static std::uint32_t hash_used(const Node &node) noexcept
    {
        return node.count >> 16;
    }
    static void set_hash_used(Node &node, std::uint32_t used) noexcept
    {
        node.count = (node.count & 0xFFFFU) | (used << 16);
    }

    struct ListBody
    {
        std::uint32_t head; // offset of the first element inside the packed run
        std::uint32_t used; // bytes of packed elements in use
    };

    static ListBody *list_body(Node &node) noexcept
    {
        return reinterpret_cast<ListBody *>(node_body(node));
    }
    static const ListBody *list_body(const Node &node) noexcept
    {
        return reinterpret_cast<const ListBody *>(node_body(node));
    }
    static std::uint32_t list_run_capacity(const Node &node) noexcept
    {
        const auto body = node_body_capacity(node);
        return body > sizeof(ListBody) ? body - sizeof(ListBody) : 0;
    }
    static char *list_run(Node &node) noexcept
    {
        return node_body(node) + sizeof(ListBody);
    }
    static const char *list_run(const Node &node) noexcept
    {
        return node_body(node) + sizeof(ListBody);
    }

    static std::unordered_map<std::string, Value> *promoted_hash(const Node &node) noexcept
    {
        std::unordered_map<std::string, Value> *table = nullptr;
        std::memcpy(&table, node_body(node), sizeof(table));
        return table;
    }

    // A list or hash element, without forcing the caller to build a Value.
    struct ElementInput
    {
        bool is_integer = false;
        std::int64_t integer = 0;
        std::string_view text;

        static ElementInput from(const Value &value) noexcept;
    };
    static Value input_to_value(const ElementInput &input);

    // --- clock ----------------------------------------------------------
    static std::int64_t steady_ms() noexcept;
    std::int64_t now_ms() const;

    // --- keyspace -------------------------------------------------------
    Node *find_node(std::string_view key) noexcept;
    Node *find_active(std::string_view key);
    Node *find_writable(std::string_view key);

    Node *create_node(std::string_view key, NodeType type, std::uint32_t body_capacity);
    Node *insert_node(std::string_view key, NodeType type, std::uint32_t body_capacity);
    Node *grow_node(Node *node, std::uint32_t new_body_capacity);
    void link(Node *node) noexcept;
    void unlink(const Node *node) noexcept;
    void destroy(Node *node) noexcept;
    void erase_node(Node *node) noexcept;
    void clear_keyspace() noexcept;
    void release_nodes() noexcept;

    void maybe_rehash();
    void rehash(std::size_t new_bucket_count);
    void touch(Node &node) noexcept;
    void unlink_lru(Node &node) noexcept;
    Node *lru_head_ = nullptr;
    Node *lru_tail_ = nullptr;

    // --- expiry side table ----------------------------------------------
    using ExpiryClock = std::int64_t; // steady milliseconds

    struct Expiry
    {
        ExpiryClock deadline;
        std::size_t index;
    };
    std::unordered_map<Node *, Expiry> expiries_;
    std::vector<Node *> expiry_heap_;
    void clear_expiry(Node &node) noexcept;
    void schedule_expiry(Node &node, ExpiryClock deadline);
    void heap_fix(std::size_t index) noexcept;
    void remove_expired_locked(std::size_t limit);
    void log(WalOp op, std::initializer_list<std::string_view> args);
    void replay(WalOp op, const std::vector<std::string_view> &args);
    std::unique_ptr<Wal> wal_;
    bool replaying_ = false;
    std::atomic<std::size_t> key_hint_{0};
    std::atomic<bool> ttl_hint_{false};

    // --- packed lists ---------------------------------------------------
    Node *list_reserve(Node *node, std::uint32_t extra_front, std::uint32_t extra_back);
    Result<std::int64_t> list_push(std::string_view key, const ElementInput &value, bool front);
    Result<std::optional<Value>> list_pop(std::string_view key, bool front);

    // --- packed / promoted hashes ---------------------------------------
    Result<std::int64_t> hash_assign(std::string_view key, std::string_view field,
                                     const ElementInput &value);
    // Locates a field in a packed hash. On success `element` points at the value
    // element and `field_stride` is the byte length of the field element.
    static bool hash_find(const Node &node, std::string_view field, const char **element,
                          std::size_t *field_stride) noexcept;
    Node *promote_hash(Node *node);

    // `with_backlen` selects the list form (traversable backwards, used by RPOP)
    // or the compact hash form.
    static std::size_t element_size(const ElementInput &value, bool with_backlen) noexcept;
    static char *write_element(char *out, const ElementInput &value, bool with_backlen) noexcept;
    static Value element_to_value(const pack::Element &element);
    static ValueView view_of(const Value &value) noexcept;
    static std::uint32_t grown_capacity(std::uint32_t required, std::uint32_t current) noexcept;

    mutable std::shared_mutex mutex_;
    SlabPool pool_;
    std::vector<Node *> buckets_;
    std::size_t key_total_ = 0;
    std::size_t max_keys_;
    std::size_t tracked_bytes_ = 0;
    std::size_t ttl_key_count_ = 0;
    std::uint64_t evictions_ = 0;
    std::uint64_t expired_keys_ = 0;
    std::uint64_t sequence_ = 0;
};

} // namespace reddish
