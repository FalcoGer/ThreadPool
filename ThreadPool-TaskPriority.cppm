module;

#include <compare>
#include <cstdint>
#include <format>
#include <functional>

export module ThreadPool:TaskPriority;

namespace ThreadPool
{
export class TaskPriority
{
  private:
    std::int16_t m_priority;

  public:
    TaskPriority() : m_priority(0)
    {
        // empty
    }
    explicit TaskPriority(const std::int16_t PRIORITY) : m_priority(PRIORITY)
    {
        // empty
    }

    [[nodiscard]]
    auto get() const noexcept -> std::int16_t
    {
        return m_priority;
    }

    [[nodiscard]]
    auto operator<=> (const TaskPriority& other) const noexcept -> std::strong_ordering
    {
        return m_priority <=> other.m_priority;
    }

    [[nodiscard]]
    auto operator== (const TaskPriority& other) const noexcept -> bool
    {
        return m_priority == other.m_priority;
    }
};
}    // namespace ThreadPool

template <>
struct std::formatter<ThreadPool::TaskPriority> : public std::formatter<std::int16_t>
{
    auto format(const ThreadPool::TaskPriority& taskPriority, format_context& ctx) const
    {
        return std::formatter<std::int16_t>::format(taskPriority.get(), ctx);
    }
};

template <>
struct std::hash<ThreadPool::TaskPriority>
{
    [[nodiscard]]
    auto operator() (const ThreadPool::TaskPriority& taskPriority) const noexcept -> std::size_t
    {
        return std::hash<std::int16_t> {}(taskPriority.get());
    }
};
