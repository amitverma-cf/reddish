#include "slab.hpp"

namespace reddish
{

using namespace slab_detail;

void SlabPool::add_chunk(std::size_t index)
{
    const std::size_t block = class_sizes[index];

    std::size_t bytes = chunk_bytes;

    while (bytes < block * 16)
        bytes *= 2;

    void *memory = memory_->allocate(bytes);
    try
    {
        auto *chunk = static_cast<Chunk *>(memory_->allocate(sizeof(Chunk), alignof(Chunk)));
        new (chunk) Chunk{memory, bytes, index, chunks_};
        chunks_ = chunk;
    }
    catch (...)
    {
        memory_->deallocate(memory, bytes);
        throw;
    }
    reserved_ += bytes;

    auto *cursor = static_cast<char *>(memory);
    const std::size_t blocks = bytes / block;
    for (std::size_t offset = 0; offset < blocks; ++offset)
    {
        auto *free_block = reinterpret_cast<FreeBlock *>(cursor + offset * block);
        free_block->next = free_lists_[index];
        free_lists_[index] = free_block;
    }
}

void *SlabPool::allocate(std::size_t size)
{
    if (size > largest_class) return memory_->allocate(size);

    const std::size_t index = class_index(size);
    if (free_lists_[index] == nullptr) add_chunk(index);

    FreeBlock *block = free_lists_[index];
    free_lists_[index] = block->next;
    return block;
}

void SlabPool::deallocate(void *block, std::size_t size) noexcept
{
    if (block == nullptr) return;

    if (size > largest_class)
    {
        memory_->deallocate(block, size);
        return;
    }

    const std::size_t index = class_index(size);
    auto *free_block = static_cast<FreeBlock *>(block);
    free_block->next = free_lists_[index];
    free_lists_[index] = free_block;
}

void SlabPool::release() noexcept
{
    while (chunks_)
    {
        auto *chunk = chunks_;
        chunks_ = chunk->next;
        memory_->deallocate(chunk->memory, chunk->bytes);
        memory_->deallocate(chunk, sizeof(Chunk), alignof(Chunk));
    }
    for (auto &list : free_lists_)
        list = nullptr;
    reserved_ = 0;
}

void SlabPool::reclaim() noexcept
{
    for (auto **entry = &chunks_; *entry;)
    {
        auto *chunk = *entry;
        const auto begin = reinterpret_cast<std::uintptr_t>(chunk->memory);
        const auto end = begin + chunk->bytes;
        std::size_t free = 0;
        for (auto *block = free_lists_[chunk->index]; block; block = block->next)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(block);
            if (address >= begin && address < end) ++free;
        }
        if (free != chunk->bytes / class_sizes[chunk->index])
        {
            entry = &chunk->next;
            continue;
        }
        for (auto **block = &free_lists_[chunk->index]; *block;)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(*block);
            if (address >= begin && address < end) *block = (*block)->next;
            else block = &(*block)->next;
        }
        *entry = chunk->next;
        reserved_ -= chunk->bytes;
        memory_->deallocate(chunk->memory, chunk->bytes);
        memory_->deallocate(chunk, sizeof(Chunk), alignof(Chunk));
    }
}

}
