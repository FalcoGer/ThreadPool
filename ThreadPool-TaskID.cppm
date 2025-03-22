module;

#include <compare>
#include <cstdint>
#include <format>
#include <functional>

export module ThreadPool:TaskID;

namespace ThreadPool
{
export class TaskID
{
  private:
    std::uint32_t m_taskId;
    explicit TaskID(std::uint32_t id) : m_taskId(id)
    {
        // empty
    }

    template <typename T>
    friend class ThreadPool;

  public:
    [[nodiscard]]
    auto get() const noexcept -> std::uint32_t
    {
        return m_taskId;
    }

    [[nodiscard]]
    friend auto operator<=> (const TaskID& lhs, const TaskID& rhs) -> std::strong_ordering
    {
        return lhs.m_taskId <=> rhs.m_taskId;
    }

    [[nodiscard]]
    friend auto operator== (const TaskID& lhs, const TaskID& rhs)
    {
        return lhs.m_taskId == rhs.m_taskId;
    }
};
}    // namespace ThreadPool

export template <>
struct std::hash<ThreadPool::TaskID>
{
    [[nodiscard]]
    auto operator()(const ThreadPool::TaskID& taskID) const noexcept -> std::size_t
    {
        return std::hash<std::uint32_t>{}(taskID.get());
    }
};

export template <>
struct std::formatter<ThreadPool::TaskID> : public std::formatter<std::uint32_t>
{
    auto format(const ThreadPool::TaskID& taskID, std::format_context& ctx) const
    {
        return std::formatter<std::uint32_t>::format(taskID.get(), ctx);
    }
};
