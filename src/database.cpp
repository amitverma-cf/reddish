#include "database.hpp"

#include "error.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace reddish
{
namespace
{
std::uint32_t hash_key(std::string_view key) noexcept
{
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const char character : key)
    {
        hash ^= static_cast<unsigned char>(character);
        hash *= 0x100000001B3ULL;
    }
    return static_cast<std::uint32_t>(hash ^ (hash >> 32));
}

std::int64_t current_unix_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}

Database::Database(std::size_t max_keys)
    : buckets_(initial_bucket_count, nullptr), max_keys_(max_keys == 0 ? 1 : max_keys)
{
}

Database::~Database()
{
    release_nodes();
}

void Database::release_nodes() noexcept
{
    for (Node *bucket : buckets_)
    {
        for (Node *node = bucket; node != nullptr;)
        {
            Node *next = node->next;
            if (node_type(*node) == NodeType::hash && (node_flags(*node) & flag_hash_promoted) != 0)
                delete promoted_hash(*node);
            if (node_size_class(*node) == oversized_class)
                pool_.deallocate(node, node_block_size(*node));
            node = next;
        }
    }
}

std::int64_t Database::steady_ms() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::int64_t Database::now_ms() const
{
    return steady_ms();
}

Database::Node *Database::find_node(std::string_view key) noexcept
{
    const std::uint32_t hash = hash_key(key);
    const std::size_t index = hash & (buckets_.size() - 1);

    for (Node *node = buckets_[index]; node != nullptr; node = node->next)
    {
        if (node_key_length(*node) != key.size()) continue;
        if (key.empty() || std::memcmp(node_key(*node), key.data(), key.size()) == 0) return node;
    }
    return nullptr;
}

Database::Node *Database::find_active(std::string_view key)
{
    Node *node = find_node(key);
    if (node == nullptr) return nullptr;

    if ((node_flags(*node) & flag_has_ttl) != 0)
    {
        const auto entry = expiries_.find(node);
        if (entry != expiries_.end() && entry->second.deadline <= now_ms() && !replaying_)
        {
            return nullptr;
        }
    }

    touch(*node);
    return node;
}

Database::Node *Database::find_writable(std::string_view key)
{
    if (key.size() > max_key_length) throw Error{ErrorCode::request_too_large};
    Node *node = find_node(key);
    if (node && !replaying_ && (node_flags(*node) & flag_has_ttl))
    {
        auto expiry = expiries_.find(node);
        if (expiry != expiries_.end() && expiry->second.deadline <= now_ms())
        {
            log(WalOp::del, {key});
            erase_node(node);
            ++expired_keys_;
            return nullptr;
        }
    }
    if (node) touch(*node);
    return node;
}

Database::Node *Database::create_node(std::string_view key, NodeType type,
                                      std::uint32_t body_capacity)
{
    const auto stride = key_stride(static_cast<std::uint32_t>(key.size()));
    std::size_t requested = sizeof(Node) + stride + body_capacity;

    std::size_t size_class = oversized_class;
    if (requested <= slab_detail::largest_class)
    {
        size_class = slab_detail::class_index(requested);
        requested = slab_detail::class_sizes[size_class];
    }
    else
    {
        requested += sizeof(std::uint64_t);
    }
    if (requested > std::numeric_limits<std::uint32_t>::max()) throw std::bad_alloc();

    const std::size_t block = requested;
    auto *node = static_cast<Node *>(pool_.allocate(block));
    node->next = nullptr;
    node->key_meta = make_key_meta(key.size(), type, 0, static_cast<std::uint8_t>(size_class));
    node->lru_previous = nullptr;
    node->lru_next = nullptr;
    node->count = 0;
    if (!key.empty()) std::memcpy(node_key(*node), key.data(), key.size());
    if (size_class == oversized_class)
    {
        const auto exact = static_cast<std::uint32_t>(block);
        std::memcpy(node_key(*node) + stride, &exact, sizeof(exact));
    }

    tracked_bytes_ += block;
    return node;
}

void Database::link(Node *node) noexcept
{
    const std::size_t index =
        hash_key({node_key(*node), node_key_length(*node)}) & (buckets_.size() - 1);
    node->next = buckets_[index];
    buckets_[index] = node;
}

void Database::unlink(const Node *node) noexcept
{
    const std::size_t index =
        hash_key({node_key(*node), node_key_length(*node)}) & (buckets_.size() - 1);
    Node **cursor = &buckets_[index];
    while (*cursor != node)
        cursor = &(*cursor)->next;
    *cursor = node->next;
}

void Database::destroy(Node *node) noexcept
{
    if (node_type(*node) == NodeType::hash && (node_flags(*node) & flag_hash_promoted) != 0)
        delete promoted_hash(*node);

    const std::size_t block = node_block_size(*node);
    tracked_bytes_ -= block;
    pool_.deallocate(node, block);
}

void Database::erase_node(Node *node) noexcept
{
    clear_expiry(*node);

    unlink_lru(*node);
    unlink(node);
    --key_total_;
    key_hint_.store(key_total_);
    destroy(node);
    ++sequence_;
}

void Database::clear_keyspace() noexcept
{
    for (Node *&bucket : buckets_)
    {
        for (Node *node = bucket; node != nullptr;)
        {
            Node *next = node->next;
            destroy(node);
            node = next;
        }
        bucket = nullptr;
    }
    lru_head_ = nullptr;
    lru_tail_ = nullptr;
    key_total_ = 0;
    ttl_key_count_ = 0;
    expiries_.clear();
    expiry_heap_.clear();
    key_hint_ = 0;
    ttl_hint_ = false;
    ++sequence_;
}

Database::Node *Database::insert_node(std::string_view key, NodeType type,
                                      std::uint32_t body_capacity)
{
    maybe_rehash();

    const std::uint32_t hash = hash_key(key);
    const std::size_t index = hash & (buckets_.size() - 1);
    Node *node = create_node(key, type, body_capacity);
    node->next = buckets_[index];
    buckets_[index] = node;
    touch(*node);
    ++key_total_;
    key_hint_.store(key_total_);
    ++sequence_;
    return node;
}

Database::Node *Database::grow_node(Node *node, std::uint32_t new_body_capacity)
{
    const std::size_t key_length = node_key_length(*node);
    const auto stride = key_stride(static_cast<std::uint32_t>(key_length));
    std::size_t requested = sizeof(Node) + stride + new_body_capacity;

    std::size_t size_class = oversized_class;
    if (requested <= slab_detail::largest_class)
    {
        size_class = slab_detail::class_index(requested);
        requested = slab_detail::class_sizes[size_class];
    }
    else
    {
        requested += sizeof(std::uint64_t);
    }
    if (requested > std::numeric_limits<std::uint32_t>::max()) throw std::bad_alloc();

    const std::size_t block = requested;
    auto *fresh = static_cast<Node *>(pool_.allocate(block));
    fresh->next = node->next;
    fresh->key_meta = make_key_meta(key_length, node_type(*node), node_flags(*node),
                                    static_cast<std::uint8_t>(size_class));
    fresh->lru_previous = node->lru_previous;
    fresh->lru_next = node->lru_next;
    if (fresh->lru_previous) fresh->lru_previous->lru_next = fresh;
    else lru_head_ = fresh;
    if (fresh->lru_next) fresh->lru_next->lru_previous = fresh;
    else lru_tail_ = fresh;
    fresh->count = node->count;
    std::memcpy(node_key(*fresh), node_key(*node), key_length);
    if (size_class == oversized_class)
    {
        const auto exact = static_cast<std::uint32_t>(block);
        std::memcpy(node_key(*fresh) + stride, &exact, sizeof(exact));
    }

    const std::size_t old_body = node_body_capacity(*node);
    const std::size_t new_body = node_body_capacity(*fresh);
    const std::size_t copy = std::min(old_body, new_body);
    if (copy != 0) std::memcpy(node_body(*fresh), node_body(*node), copy);

    if (node_flags(*node) & flag_has_ttl)
    {
        auto entry = expiries_.extract(node);
        expiry_heap_[entry.mapped().index] = fresh;
        entry.key() = fresh;
        expiries_.insert(std::move(entry));
    }

    const std::size_t index =
        hash_key({node_key(*fresh), node_key_length(*fresh)}) & (buckets_.size() - 1);
    if (buckets_[index] == node)
    {
        buckets_[index] = fresh;
    }
    else
    {
        Node *cursor = buckets_[index];
        while (cursor->next != node)
            cursor = cursor->next;
        cursor->next = fresh;
    }

    tracked_bytes_ -= node_block_size(*node);
    tracked_bytes_ += block;
    pool_.deallocate(node, node_block_size(*node));
    ++sequence_;
    return fresh;
}

std::uint32_t Database::grown_capacity(std::uint32_t required, std::uint32_t current) noexcept
{
    std::uint32_t capacity = std::max<std::uint32_t>(required, 32);
    if (current != 0)
    {
        const std::uint32_t doubled = current * 2;
        if (doubled > capacity) capacity = doubled;
    }
    if (capacity < required) capacity = required;
    return capacity;
}

void Database::maybe_rehash()
{
    if (key_total_ + 1 <= buckets_.size()) return;
    if (buckets_.size() >= (std::size_t{1} << 31)) return;
    rehash(buckets_.size() * 2);
}

void Database::rehash(std::size_t new_bucket_count)
{
    std::vector<Node *> fresh(new_bucket_count, nullptr);
    const std::size_t mask = new_bucket_count - 1;
    for (Node *node : buckets_)
    {
        while (node != nullptr)
        {
            Node *next = node->next;
            const std::size_t index = hash_key({node_key(*node), node_key_length(*node)}) & mask;
            node->next = fresh[index];
            fresh[index] = node;
            node = next;
        }
    }
    buckets_.swap(fresh);
}

void Database::unlink_lru(Node &node) noexcept
{
    if (node.lru_previous) node.lru_previous->lru_next = node.lru_next;
    else lru_head_ = node.lru_next;
    if (node.lru_next) node.lru_next->lru_previous = node.lru_previous;
    else lru_tail_ = node.lru_previous;
    node.lru_previous = nullptr;
    node.lru_next = nullptr;
}
void Database::touch(Node &node) noexcept
{
    if (lru_head_ == &node) return;
    if (node.lru_previous || node.lru_next || lru_tail_ == &node) unlink_lru(node);
    node.lru_next = lru_head_;
    node.lru_previous = nullptr;
    if (lru_head_) lru_head_->lru_previous = &node;
    else lru_tail_ = &node;
    lru_head_ = &node;
}

std::size_t Database::evict_to(std::size_t target, std::size_t limit)
{
    std::unique_lock guard(mutex_);
    std::size_t evicted = 0;
    begin_wal_batch();
    try
    {
        while (evicted < limit && key_total_ > target)
        {
            Node *victim = lru_tail_;
            if (victim == nullptr) break;
            log(WalOp::del, {std::string_view(node_key(*victim), node_key_length(*victim))});
            erase_node(victim);
            ++evictions_;
            ++evicted;
        }
    }
    catch (...)
    {
        end_wal_batch();
        throw;
    }
    end_wal_batch();
    return evicted;
}

void Database::set(std::string_view key, std::string_view val)
{
    if (val.size() > 64 * 1024 * 1024) throw Error{ErrorCode::request_too_large};
    Node *node = find_writable(key);
    if (node == nullptr || node_type(*node) != NodeType::string ||
        node_body_capacity(*node) < val.size() ||
        (node_body_capacity(*node) > SlabPool::largest_class &&
         val.size() < node_body_capacity(*node) / 4))
    {
        if (node == nullptr) maybe_rehash();
        auto cleanup = [this](Node *pending) { destroy(pending); };
        std::unique_ptr<Node, decltype(cleanup)> pending(
            create_node(key, NodeType::string, static_cast<std::uint32_t>(val.size())), cleanup);
        if (!val.empty()) std::memcpy(node_body(*pending), val.data(), val.size());
        pending->count = static_cast<std::uint32_t>(val.size());
        log(WalOp::set, {key, val});
        if (node) erase_node(node);
        node = pending.release();
        link(node);
        touch(*node);
        ++key_total_;
        key_hint_.store(key_total_);
        ++sequence_;
        return;
    }

    log(WalOp::set, {key, val});
    if (!val.empty()) std::memmove(node_body(*node), val.data(), val.size());
    node->count = static_cast<std::uint32_t>(val.size());
    touch(*node);
    clear_expiry(*node);
    ++sequence_;
}

void Database::heap_fix(std::size_t index) noexcept
{
    auto less = [&](std::size_t a, std::size_t b)
    {
        return expiries_.find(expiry_heap_[a])->second.deadline <
               expiries_.find(expiry_heap_[b])->second.deadline;
    };
    auto swap = [&](std::size_t a, std::size_t b)
    {
        std::swap(expiry_heap_[a], expiry_heap_[b]);
        expiries_.find(expiry_heap_[a])->second.index = a;
        expiries_.find(expiry_heap_[b])->second.index = b;
    };
    while (index && less(index, (index - 1) / 2))
    {
        const auto parent = (index - 1) / 2;
        swap(index, parent);
        index = parent;
    }
    while (index * 2 + 1 < expiry_heap_.size())
    {
        auto child = index * 2 + 1;
        if (child + 1 < expiry_heap_.size() && less(child + 1, child)) ++child;
        if (!less(child, index)) break;
        swap(child, index);
        index = child;
    }
}
void Database::clear_expiry(Node &node) noexcept
{
    auto entry = expiries_.find(&node);
    if (entry == expiries_.end()) return;
    const auto index = entry->second.index;
    Node *last = expiry_heap_.back();
    expiry_heap_.pop_back();
    expiries_.erase(entry);
    if (index < expiry_heap_.size())
    {
        expiry_heap_[index] = last;
        expiries_.find(last)->second.index = index;
        heap_fix(index);
    }
    set_flags(node, static_cast<std::uint8_t>(node_flags(node) & ~flag_has_ttl));
    ttl_key_count_ = expiry_heap_.size();
    ttl_hint_ = ttl_key_count_ != 0;
}
void Database::schedule_expiry(Node &node, ExpiryClock deadline)
{
    auto entry = expiries_.find(&node);
    if (entry == expiries_.end())
    {
        if (expiry_heap_.size() == expiry_heap_.capacity())
            expiry_heap_.reserve(std::max<std::size_t>(16, expiry_heap_.size() * 2));
        entry = expiries_.emplace(&node, Expiry{deadline, expiry_heap_.size()}).first;
        expiry_heap_.push_back(&node);
    }
    else entry->second.deadline = deadline;
    set_flags(node, static_cast<std::uint8_t>(node_flags(node) | flag_has_ttl));
    heap_fix(entry->second.index);
    ttl_key_count_ = expiry_heap_.size();
    ttl_hint_ = true;
}

Result<std::optional<std::string>> Database::get(std::string_view key)
{
    auto view = value_view(key);
    if (!view) return std::unexpected(view.error());
    if (!*view) return std::nullopt;

    if ((*view)->kind == ValueView::Kind::integer) return std::to_string((*view)->integer);
    if ((*view)->kind != ValueView::Kind::string)
        return std::unexpected(Error{ErrorCode::wrong_type});
    return std::string{(*view)->text};
}

Result<std::optional<ValueView>> Database::value_view(std::string_view key)
{
    Node *node = find_active(key);
    if (node == nullptr) return std::nullopt;

    ValueView view;
    switch (node_type(*node))
    {
    case NodeType::string:
        view.kind = ValueView::Kind::string;
        view.text = string_value(*node);
        return view;
    case NodeType::integer:
        view.kind = ValueView::Kind::integer;
        view.integer = integer_value(*node);
        return view;
    case NodeType::list:
        view.kind = ValueView::Kind::list;
        return view;
    default:
        view.kind = ValueView::Kind::hash;
        return view;
    }
}

Result<std::int64_t> Database::increment(std::string_view key)
{
    Node *node = find_writable(key);
    if (node == nullptr)
    {
        log(WalOp::increment, {key});
        node =
            insert_node(key, NodeType::integer, static_cast<std::uint32_t>(sizeof(std::int64_t)));
        set_integer_value(*node, 1);
        return 1;
    }

    if (node_type(*node) == NodeType::integer)
    {
        const std::int64_t current = integer_value(*node);
        if (current == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(Error{ErrorCode::value_not_integer});
        log(WalOp::increment, {key});
        set_integer_value(*node, current + 1);
        ++sequence_;
        return current + 1;
    }

    if (node_type(*node) != NodeType::string) return std::unexpected(Error{ErrorCode::wrong_type});

    const auto text = string_value(*node);
    std::int64_t current = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), current);
    if (error != std::errc{} || end != text.data() + text.size() ||
        current == std::numeric_limits<std::int64_t>::max())
        return std::unexpected(Error{ErrorCode::value_not_integer});

    log(WalOp::increment, {key});
    if (node_body_capacity(*node) < sizeof(std::int64_t))
        node = grow_node(node, sizeof(std::int64_t));
    set_type(*node, NodeType::integer);
    node->count = 0;
    set_integer_value(*node, current + 1);
    ++sequence_;
    return current + 1;
}

std::size_t Database::element_size(const ElementInput &value, bool with_backlen) noexcept
{
    if (value.is_integer)
        return with_backlen ? pack::encoded_size(value.integer) : pack::body_size(value.integer);
    return with_backlen ? pack::encoded_size(value.text) : pack::body_size(value.text);
}

char *Database::write_element(char *out, const ElementInput &value, bool with_backlen) noexcept
{
    if (value.is_integer)
        return with_backlen ? pack::encode(out, value.integer)
                            : pack::encode_body(out, value.integer);
    return with_backlen ? pack::encode(out, value.text) : pack::encode_body(out, value.text);
}

Database::ElementInput Database::ElementInput::from(const Value &value) noexcept
{
    ElementInput input;
    if (const auto *integer = std::get_if<std::int64_t>(&value))
    {
        input.is_integer = true;
        input.integer = *integer;
        return input;
    }
    input.text = std::get<std::string>(value);
    return input;
}

Value Database::input_to_value(const ElementInput &input)
{
    if (input.is_integer) return Value{input.integer};
    return Value{std::string{input.text}};
}

Value Database::element_to_value(const pack::Element &element)
{
    if (element.is_integer()) return Value{element.integer};
    return Value{std::string{element.text}};
}

Database::Node *Database::list_reserve(Node *node, std::uint32_t extra_front,
                                       std::uint32_t extra_back)
{
    ListBody *body = list_body(*node);
    const std::uint32_t capacity = list_run_capacity(*node);
    const bool front_fits = body->head >= extra_front;
    const bool back_fits = body->head + body->used + extra_back <= capacity;
    if (front_fits && back_fits) return node;

    const std::uint32_t required = body->used + extra_front + extra_back;
    const std::uint32_t new_capacity =
        required <= capacity ? capacity : grown_capacity(required, capacity);
    const std::uint32_t used = body->used;

    if (new_capacity > capacity)
        node = grow_node(node, static_cast<std::uint32_t>(sizeof(ListBody) + new_capacity));
    body = list_body(*node);

    const std::uint32_t lowest = extra_front;
    const std::uint32_t highest = new_capacity - used - extra_back;
    std::uint32_t new_head = (new_capacity - used) / 2;
    if (new_head < lowest) new_head = lowest;
    if (new_head > highest) new_head = highest;

    if (new_head != body->head && used != 0)
        std::memmove(list_run(*node) + new_head, list_run(*node) + body->head, used);
    body->head = new_head;
    return node;
}

Result<std::int64_t> Database::list_push(std::string_view key, const ElementInput &value,
                                         bool front)
{
    const auto encoded = static_cast<std::uint32_t>(element_size(value, true));

    Node *node = find_writable(key);
    if (node && node_type(*node) != NodeType::list)
        return std::unexpected(Error{ErrorCode::wrong_type});
    if (node && std::uint64_t(list_body(*node)->used) + encoded > 64 * 1024 * 1024)
        return std::unexpected(Error{ErrorCode::request_too_large});
    if (element_size(value, true) > 64 * 1024 * 1024)
        return std::unexpected(Error{ErrorCode::request_too_large});
    const auto integer = value.is_integer ? std::to_string(value.integer) : std::string{};
    log(front ? WalOp::push_left : WalOp::push_right,
        {key, value.is_integer ? "i" : "s",
         value.is_integer ? std::string_view(integer) : value.text});
    if (node == nullptr)
    {
        node = insert_node(key, NodeType::list,
                           static_cast<std::uint32_t>(sizeof(ListBody) + encoded));
        list_body(*node)->head = 0;
        list_body(*node)->used = 0;
    }
    else if (node_type(*node) != NodeType::list)
    {
        return std::unexpected(Error{ErrorCode::wrong_type});
    }

    const bool use_front = front && node->count != 0;
    node = list_reserve(node, use_front ? encoded : 0, use_front ? 0 : encoded);

    ListBody *body = list_body(*node);
    char *run = list_run(*node);
    if (use_front)
    {
        body->head -= encoded;
        write_element(run + body->head, value, true);
    }
    else
    {
        write_element(run + body->head + body->used, value, true);
    }
    body->used += encoded;
    ++node->count;
    touch(*node);
    ++sequence_;
    return static_cast<std::int64_t>(node->count);
}

Result<std::int64_t> Database::push_left(std::string_view key, Value value)
{
    return list_push(key, ElementInput::from(value), true);
}

Result<std::int64_t> Database::push_right(std::string_view key, Value value)
{
    return list_push(key, ElementInput::from(value), false);
}

Result<std::int64_t> Database::push_left_text(std::string_view key, std::string_view value)
{
    ElementInput input;
    input.text = value;
    return list_push(key, input, true);
}

Result<std::int64_t> Database::push_right_text(std::string_view key, std::string_view value)
{
    ElementInput input;
    input.text = value;
    return list_push(key, input, false);
}

Result<std::optional<Value>> Database::list_pop(std::string_view key, bool front)
{
    Node *node = find_writable(key);
    if (node == nullptr) return std::nullopt;
    if (node_type(*node) != NodeType::list) return std::unexpected(Error{ErrorCode::wrong_type});
    if (node->count == 0) return std::nullopt;

    ListBody *body = list_body(*node);
    char *run = list_run(*node);
    Value result;

    std::uint32_t removed = 0;
    if (front)
    {
        const char *element = run + body->head;
        removed = static_cast<std::uint32_t>(pack::stride(element));
        result = element_to_value(pack::decode(element));
    }
    else
    {
        const char *end = run + body->head + body->used;
        const std::size_t payload = pack::decode_backlen(end);
        const std::size_t full = payload + pack::backlen_bytes(payload);
        result = element_to_value(pack::decode(end - full));
        removed = static_cast<std::uint32_t>(full);
    }

    log(front ? WalOp::pop_left : WalOp::pop_right, {key});
    if (front) body->head += removed;
    body->used -= removed;
    --node->count;
    ++sequence_;
    if (node->count == 0) erase_node(node);
    else if (list_run_capacity(*node) > SlabPool::largest_class &&
             body->used < list_run_capacity(*node) / 4)
    {
        const auto used = body->used;
        if (used) std::memmove(list_run(*node), list_run(*node) + body->head, used);
        body->head = 0;
        try
        {
            (void)grow_node(node,
                            static_cast<std::uint32_t>(sizeof(ListBody) + std::max(used, 32U)));
        }
        catch (const std::bad_alloc &)
        {
        }
    }
    return std::optional<Value>{std::move(result)};
}

Result<std::optional<Value>> Database::pop_left(std::string_view key)
{
    return list_pop(key, true);
}

Result<std::optional<Value>> Database::pop_right(std::string_view key)
{
    return list_pop(key, false);
}

Result<std::int64_t> Database::list_length(std::string_view key)
{
    Node *node = find_active(key);
    if (node == nullptr) return 0;
    if (node_type(*node) != NodeType::list) return std::unexpected(Error{ErrorCode::wrong_type});
    return static_cast<std::int64_t>(node->count);
}

ValueView Database::view_of(const Value &value) noexcept
{
    ValueView view;
    if (const auto *integer = std::get_if<std::int64_t>(&value))
    {
        view.kind = ValueView::Kind::integer;
        view.integer = *integer;
        return view;
    }
    view.kind = ValueView::Kind::string;
    view.text = std::get<std::string>(value);
    return view;
}

bool Database::hash_find(const Node &node, std::string_view field, const char **element,
                         std::size_t *field_stride) noexcept
{
    const char *cursor = node_body(node);
    const std::uint32_t count = hash_count(node);

    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto decoded = pack::decode(cursor);
        const std::size_t field_bytes = pack::element_size(cursor);
        cursor += field_bytes;
        const std::size_t value_bytes = pack::element_size(cursor);

        if (decoded.kind == pack::Kind::string && decoded.text == field)
        {
            *element = cursor;
            *field_stride = field_bytes;
            return true;
        }
        cursor += value_bytes;
    }
    return false;
}

Database::Node *Database::promote_hash(Node *node)
{
    using Table = std::unordered_map<std::string, Value>;

    auto table = std::make_unique<Table>();
    table->reserve(hash_count(*node) + 1);

    const char *cursor = node_body(*node);
    const std::uint32_t count = hash_count(*node);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto field = pack::decode(cursor);
        cursor += pack::element_size(cursor);
        const auto value = pack::decode(cursor);
        cursor += pack::element_size(cursor);
        table->emplace(std::string{field.text}, element_to_value(value));
    }

    node = grow_node(node, static_cast<std::uint32_t>(sizeof(Table *)));
    set_flags(*node, static_cast<std::uint8_t>(node_flags(*node) | flag_hash_promoted));
    node->count = static_cast<std::uint32_t>(table->size());
    Table *raw = table.release();
    std::memcpy(node_body(*node), &raw, sizeof(raw));
    return node;
}

Result<std::int64_t> Database::hash_assign(std::string_view key, std::string_view field,
                                           const ElementInput &value)
{
    Node *node = find_writable(key);
    if (node != nullptr && node_type(*node) != NodeType::hash)
        return std::unexpected(Error{ErrorCode::wrong_type});

    if (field.size() > 64 * 1024 * 1024 || element_size(value, false) > 64 * 1024 * 1024)
        return std::unexpected(Error{ErrorCode::request_too_large});
    const auto integer = value.is_integer ? std::to_string(value.integer) : std::string{};
    log(WalOp::hash_set, {key, field, value.is_integer ? "i" : "s",
                          value.is_integer ? std::string_view(integer) : value.text});
    const auto field_bytes = static_cast<std::uint32_t>(pack::body_size(field));
    const auto value_bytes = static_cast<std::uint32_t>(element_size(value, false));

    if (node == nullptr)
    {
        if (field_bytes + value_bytes > hash_packed_byte_limit)
        {
            auto table = std::make_unique<std::unordered_map<std::string, Value>>();
            table->emplace(std::string(field), input_to_value(value));
            node = insert_node(key, NodeType::hash, sizeof(void *));
            set_flags(*node, flag_hash_promoted);
            node->count = 1;
            auto *raw = table.release();
            std::memcpy(node_body(*node), &raw, sizeof(raw));
            return 1;
        }
        node = insert_node(key, NodeType::hash, field_bytes + value_bytes);
        char *out = pack::encode_body(node_body(*node), field);
        write_element(out, value, false);
        set_hash_count(*node, 1);
        set_hash_used(*node, field_bytes + value_bytes);
        return 1;
    }

    touch(*node);

    if ((node_flags(*node) & flag_hash_promoted) != 0)
    {
        auto *table = promoted_hash(*node);
        auto existing = table->find(std::string{field});
        const bool inserted = existing == table->end();
        if (inserted)
        {
            table->emplace(std::string{field}, input_to_value(value));
            node->count = static_cast<std::uint32_t>(table->size());
        }
        else
        {
            existing->second = input_to_value(value);
        }
        ++sequence_;
        return inserted ? 1 : 0;
    }

    const char *value_element = nullptr;
    std::size_t field_stride = 0;

    if (hash_find(*node, field, &value_element, &field_stride))
    {
        const std::size_t old_value_bytes = pack::element_size(value_element);
        const auto delta =
            static_cast<std::ptrdiff_t>(value_bytes) - static_cast<std::ptrdiff_t>(old_value_bytes);

        if (delta > 0 &&
            hash_used(*node) + static_cast<std::size_t>(delta) > hash_packed_byte_limit)
        {
            node = promote_hash(node);
            auto *table = promoted_hash(*node);
            table->find(std::string(field))->second = input_to_value(value);
            ++sequence_;
            return 0;
        }
        if (delta != 0)
        {
            const std::uint32_t used = hash_used(*node);
            if (delta > 0 && used + static_cast<std::uint32_t>(delta) > node_body_capacity(*node))
            {
                node = grow_node(node, grown_capacity(used + static_cast<std::uint32_t>(delta),
                                                      node_body_capacity(*node)));
                hash_find(*node, field, &value_element, &field_stride);
            }

            auto *target = const_cast<char *>(value_element);
            const std::size_t offset = static_cast<std::size_t>(target - node_body(*node));
            const std::size_t tail = used - offset - old_value_bytes;
            if (tail != 0) std::memmove(target + value_bytes, target + old_value_bytes, tail);
            set_hash_used(*node, static_cast<std::uint32_t>(used + delta));
        }

        write_element(const_cast<char *>(value_element), value, false);
        ++sequence_;
        return 0;
    }

    const std::uint32_t used = hash_used(*node);
    const std::uint32_t needed = used + field_bytes + value_bytes;
    const std::uint32_t count = hash_count(*node) + 1;

    if (count > hash_packed_limit || needed > hash_packed_byte_limit)
    {
        node = promote_hash(node);
        auto *table = promoted_hash(*node);
        table->emplace(std::string{field}, input_to_value(value));
        node->count = static_cast<std::uint32_t>(table->size());
        ++sequence_;
        return 1;
    }

    if (needed > node_body_capacity(*node))
        node = grow_node(node, grown_capacity(needed, node_body_capacity(*node)));

    char *out = node_body(*node) + used;
    char *end = pack::encode_body(out, field);
    write_element(end, value, false);
    set_hash_count(*node, count);
    set_hash_used(*node, needed);
    ++sequence_;
    return 1;
}

Result<std::int64_t> Database::hash_set(std::string_view key, std::string_view field, Value value)
{
    return hash_assign(key, field, ElementInput::from(value));
}

Result<std::int64_t> Database::hash_set_text(std::string_view key, std::string_view field,
                                             std::string_view value)
{
    ElementInput input;
    input.text = value;
    return hash_assign(key, field, input);
}

Result<std::optional<ValueView>> Database::hash_field(std::string_view key, std::string_view field)
{
    Node *node = find_active(key);
    if (node == nullptr) return std::nullopt;
    if (node_type(*node) != NodeType::hash) return std::unexpected(Error{ErrorCode::wrong_type});

    if ((node_flags(*node) & flag_hash_promoted) != 0)
    {
        const auto *table = promoted_hash(*node);
        const auto found = table->find(std::string{field});
        if (found == table->end()) return std::nullopt;
        return view_of(found->second);
    }

    const char *element = nullptr;
    std::size_t field_stride = 0;
    if (!hash_find(*node, field, &element, &field_stride)) return std::nullopt;

    const auto decoded = pack::decode(element);
    ValueView view;
    if (decoded.is_integer())
    {
        view.kind = ValueView::Kind::integer;
        view.integer = decoded.integer;
    }
    else
    {
        view.kind = ValueView::Kind::string;
        view.text = decoded.text;
    }
    return view;
}

Result<std::optional<Value>> Database::hash_get(std::string_view key, std::string_view field)
{
    auto view = hash_field(key, field);
    if (!view) return std::unexpected(view.error());
    if (!*view) return std::nullopt;
    if ((*view)->kind == ValueView::Kind::integer) return Value{(*view)->integer};
    return Value{std::string{(*view)->text}};
}

Result<bool> Database::hash_del(std::string_view key, std::string_view field)
{
    Node *node = find_writable(key);
    if (node == nullptr) return false;
    if (node_type(*node) != NodeType::hash) return std::unexpected(Error{ErrorCode::wrong_type});

    if ((node_flags(*node) & flag_hash_promoted) != 0)
    {
        auto *table = promoted_hash(*node);
        auto entry = table->find(std::string(field));
        if (entry == table->end()) return false;
        log(WalOp::hash_del, {key, field});
        table->erase(entry);
        const bool removed = true;
        if (!removed) return false;
        node->count = static_cast<std::uint32_t>(table->size());
        ++sequence_;
        if (table->empty()) erase_node(node);
        return true;
    }

    const char *element = nullptr;
    std::size_t field_stride = 0;
    if (!hash_find(*node, field, &element, &field_stride)) return false;

    log(WalOp::hash_del, {key, field});
    const std::uint32_t used = hash_used(*node);
    const std::size_t pair_bytes = field_stride + pack::element_size(element);
    auto *pair_start = const_cast<char *>(element) - field_stride;
    const std::size_t offset = static_cast<std::size_t>(pair_start - node_body(*node));
    const std::size_t tail = used - offset - pair_bytes;
    if (tail != 0) std::memmove(pair_start, pair_start + pair_bytes, tail);

    const std::uint32_t count = hash_count(*node);
    set_hash_used(*node, static_cast<std::uint32_t>(used - pair_bytes));
    set_hash_count(*node, count - 1);
    ++sequence_;

    if (count == 1) erase_node(node);
    return true;
}

Result<std::int64_t> Database::hash_length(std::string_view key)
{
    Node *node = find_active(key);
    if (node == nullptr) return 0;
    if (node_type(*node) != NodeType::hash) return std::unexpected(Error{ErrorCode::wrong_type});

    if ((node_flags(*node) & flag_hash_promoted) != 0)
        return static_cast<std::int64_t>(promoted_hash(*node)->size());
    return static_cast<std::int64_t>(hash_count(*node));
}

bool Database::expire(std::string_view key, std::int64_t seconds)
{
    Node *node = find_writable(key);
    if (!node) return false;
    if (seconds <= 0)
    {
        log(WalOp::del, {key});
        erase_node(node);
        return true;
    }
    const auto now = now_ms();
    const auto max_seconds =
        (std::numeric_limits<std::int64_t>::max() - std::max(now, current_unix_ms())) / 1000;
    if (seconds > max_seconds) throw Error{ErrorCode::value_not_integer};
    const auto deadline = now + seconds * 1000;
    const auto wall = current_unix_ms() + seconds * 1000;
    const auto stamp = std::to_string(wall);
    log(WalOp::expire_at, {key, stamp});
    schedule_expiry(*node, deadline);
    ++sequence_;
    return true;
}
std::int64_t Database::ttl(std::string_view key)
{
    Node *node = find_active(key);
    if (!node) return -2;
    auto entry = expiries_.find(node);
    if (entry == expiries_.end()) return -1;
    const auto remaining = entry->second.deadline - now_ms();
    return remaining <= 0 ? -2 : remaining / 1000;
}
void Database::remove_expired_locked(std::size_t limit)
{
    const auto now = now_ms();
    begin_wal_batch();
    try
    {
        for (std::size_t removed = 0; removed < limit && !expiry_heap_.empty(); ++removed)
        {
            Node *node = expiry_heap_.front();
            if (expiries_.find(node)->second.deadline > now) break;
            log(WalOp::del, {std::string_view(node_key(*node), node_key_length(*node))});
            erase_node(node);
            ++expired_keys_;
        }
    }
    catch (...)
    {
        end_wal_batch();
        throw;
    }
    end_wal_batch();
}
void Database::remove_expired(std::size_t limit)
{
    if (!ttl_hint_) return;
    std::unique_lock guard(mutex_);
    remove_expired_locked(limit);
}

bool Database::del(std::string_view key)
{
    Node *node = find_writable(key);
    if (node == nullptr) return false;
    log(WalOp::del, {key});
    erase_node(node);
    return true;
}

DatabaseStats Database::stats() const
{
    std::size_t active = tracked_bytes_ + buckets_.capacity() * sizeof(Node *);
    for (const auto *bucket : buckets_)
        for (auto *node = bucket; node; node = node->next)
        {
            if (node_type(*node) == NodeType::hash && (node_flags(*node) & flag_hash_promoted))
            {
                const auto *table = promoted_hash(*node);
                active += sizeof(*table) + table->bucket_count() * sizeof(void *);
                for (const auto &[field, value] : *table)
                {
                    active += sizeof(std::pair<const std::string, Value>) + 2 * sizeof(void *) +
                              field.capacity() + 1;
                    if (auto *text = std::get_if<std::string>(&value))
                        active += text->capacity() + 1;
                }
            }
        }
    active += expiries_.bucket_count() * sizeof(void *) +
              expiries_.size() * (sizeof(Expiry) + 3 * sizeof(void *)) +
              expiry_heap_.capacity() * sizeof(Node *);
    std::size_t slab_live = 0;
    for (auto *bucket : buckets_)
        for (auto *node = bucket; node; node = node->next)
            if (node_size_class(*node) != oversized_class) slab_live += node_block_size(*node);
    return {key_total_,
            active,
            wal_ ? wal_->bytes() : 0,
            evictions_,
            expired_keys_,
            active + pool_.reserved_bytes() - std::min(slab_live, pool_.reserved_bytes())};
}

void Database::log(WalOp op, std::initializer_list<std::string_view> args)
{
    if (wal_ && !replaying_) wal_->append(op, args);
}
void Database::flush_wal()
{
    if (wal_) wal_->flush();
}
void Database::open_wal(const std::filesystem::path &path, WalSync sync)
{
    std::unique_lock guard(mutex_);
    if (wal_ || key_total_ != 0) throw Error{ErrorCode::disk_read_failed};
    replaying_ = true;
    try
    {
        wal_ = std::make_unique<Wal>(path, sync,
                                     [this](WalOp op, const auto &args) { replay(op, args); });
    }
    catch (...)
    {
        replaying_ = false;
        clear_keyspace();
        throw;
    }
    replaying_ = false;
    remove_expired_locked(no_expiry_limit);
}
void Database::replay(WalOp op, const std::vector<std::string_view> &a)
{
    auto integer = [](std::string_view text)
    {
        std::int64_t n;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
        if (ec != std::errc{} || end != text.data() + text.size())
            throw Error{ErrorCode::disk_read_failed};
        return n;
    };
    auto value = [&](std::size_t index) -> Value
    {
        if (a[index] == "i") return integer(a[index + 1]);
        if (a[index] != "s") throw Error{ErrorCode::disk_read_failed};
        return std::string(a[index + 1]);
    };
    const auto expected = op == WalOp::hash_set                                 ? 4U
                          : (op == WalOp::push_left || op == WalOp::push_right) ? 3U
                          : (op == WalOp::set || op == WalOp::integer_set ||
                             op == WalOp::hash_del || op == WalOp::expire_at)
                              ? 2U
                              : 1U;
    if (a.size() != expected || a.empty() || a[0].size() > max_key_length)
        throw Error{ErrorCode::disk_read_failed};
    switch (op)
    {
    case WalOp::set:
        set(a[0], a[1]);
        break;
    case WalOp::integer_set:
    {
        const auto n = integer(a[1]);
        if (auto *old = find_node(a[0])) erase_node(old);
        auto *node = insert_node(a[0], NodeType::integer, sizeof(n));
        set_integer_value(*node, n);
        break;
    }
    case WalOp::increment:
        if (!increment(a[0])) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::push_left:
        if (!push_left(a[0], value(1))) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::push_right:
        if (!push_right(a[0], value(1))) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::pop_left:
        if (!pop_left(a[0])) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::pop_right:
        if (!pop_right(a[0])) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::hash_set:
        if (!hash_set(a[0], a[1], value(2))) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::hash_del:
        if (!hash_del(a[0], a[1])) throw Error{ErrorCode::disk_read_failed};
        break;
    case WalOp::expire_at:
    {
        auto *node = find_node(a[0]);
        if (!node) throw Error{ErrorCode::disk_read_failed};
        const auto wall = integer(a[1]);
        const auto current = current_unix_ms();
        if (wall < 0) throw Error{ErrorCode::disk_read_failed};
        const auto remaining = wall > current ? wall - current : 0;
        schedule_expiry(*node, now_ms() + remaining);
        break;
    }
    case WalOp::del:
        (void)del(a[0]);
        break;
    default:
        throw Error{ErrorCode::disk_read_failed};
    }
}

}
