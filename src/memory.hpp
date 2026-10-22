#pragma once
#include <memory_resource>

namespace reddish
{
class MemoryLimit : public std::bad_alloc
{
  public:
    const char *what() const noexcept override
    {
        return "Database memory limit exceeded";
    }
};

class MemoryResource final : public std::pmr::memory_resource
{
  public:
    explicit MemoryResource(std::size_t limit = 0) : limit_(limit) {}
    std::size_t used() const noexcept
    {
        return used_;
    }
    std::size_t limit() const noexcept
    {
        return limit_;
    }
    void *context = nullptr;
    void (*reclaim)(void *) = nullptr;

  private:
    void *do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        if (limit_ && bytes > limit_ - used_)
        {
            if (reclaim) reclaim(context);
            if (bytes > limit_ - used_) throw MemoryLimit{};
        }
        void *p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        used_ += bytes;
        return p;
    }
    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override
    {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
        used_ -= bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override
    {
        return this == &other;
    }
    std::size_t limit_;
    std::size_t used_ = 0;
};
}
