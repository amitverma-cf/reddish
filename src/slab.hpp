#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <vector>

namespace reddish
{

namespace slab_detail
{

inline constexpr std::size_t class_sizes[] = {
    8,   16,  24,  32,   40,   48,   56,   64,   72,   80,   88,   96,
    104, 112, 120, 128,  160,  192,  224,  256,  320,  384,  448,  512,
    640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2560, 3072, 3584, 4096,
};

inline constexpr std::size_t class_count = std::size(class_sizes);
inline constexpr std::size_t dense_limit = 128;
inline constexpr std::size_t dense_classes = dense_limit / 8;
inline constexpr std::size_t largest_class = class_sizes[class_count - 1];

inline std::size_t class_index(std::size_t size) noexcept
{
    if (size <= dense_limit) return (size <= 8) ? 0 : (size + 7) / 8 - 1;

    std::size_t low = dense_classes;
    std::size_t high = class_count - 1;
    while (low < high)
    {
        const std::size_t middle = (low + high) / 2;
        if (class_sizes[middle] < size) low = middle + 1;
        else high = middle;
    }
    return low;
}

}

class SlabPool
{
  public:
    explicit SlabPool(std::pmr::memory_resource *memory = std::pmr::get_default_resource())
        : memory_(memory)
    {
    }
    ~SlabPool()
    {
        release();
    }

    SlabPool(const SlabPool &) = delete;
    SlabPool &operator=(const SlabPool &) = delete;

    void *allocate(std::size_t size);
    void deallocate(void *block, std::size_t size) noexcept;
    void reclaim() noexcept;

    std::size_t reserved_bytes() const noexcept
    {
        return reserved_;
    }

    static constexpr std::size_t largest_class = slab_detail::largest_class;
    static constexpr std::size_t class_count = slab_detail::class_count;

    static constexpr std::size_t class_size(std::size_t index) noexcept
    {
        return slab_detail::class_sizes[index];
    }

  private:
    struct FreeBlock
    {
        FreeBlock *next;
    };

    struct Chunk
    {
        void *memory;
        std::size_t bytes;
        std::size_t index;
        Chunk *next;
    };

    static constexpr std::size_t chunk_bytes = 64 * 1024;

    void add_chunk(std::size_t index);
    void release() noexcept;

    FreeBlock *free_lists_[class_count] = {};
    Chunk *chunks_ = nullptr;
    std::pmr::memory_resource *memory_;
    std::size_t reserved_ = 0;
};
}
