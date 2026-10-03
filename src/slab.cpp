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

    void *memory = ::operator new(bytes);
    try
    {
        chunks_.push_back({memory, bytes});
    }
    catch (...)
    {
        ::operator delete(memory);
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
    if (size > largest_class) return ::operator new(size);

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
        ::operator delete(block);
        return;
    }

    const std::size_t index = class_index(size);
    auto *free_block = static_cast<FreeBlock *>(block);
    free_block->next = free_lists_[index];
    free_lists_[index] = free_block;
}

void SlabPool::release() noexcept
{
    for (const auto &chunk : chunks_)
        ::operator delete(chunk.memory);
    chunks_.clear();
    for (auto &list : free_lists_)
        list = nullptr;
    reserved_ = 0;
}

}
