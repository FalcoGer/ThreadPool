module;

#include <atomic>
#include <compare>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>

export module ThreadPool:TaskID;

import :ETaskState;

namespace ThreadPool
{

export class TaskID
{
  private:
    std::uint32_t                            m_taskId;
    std::shared_ptr<std::atomic<ETaskState>> m_taskState;

  protected:
    explicit TaskID(std::uint32_t id, std::shared_ptr<std::atomic<ETaskState>> taskState)
            : m_taskId(id), m_taskState(std::move(taskState))
    {
        if (m_taskState == nullptr)
        {
            throw std::invalid_argument("taskState shared ptr was null");
        }
    }

    void setStarted() noexcept { m_taskState->store(ETaskState::STARTED); }
    void setFinished() noexcept { m_taskState->store(ETaskState::FINISHED); }
    void setCanceled() noexcept { m_taskState->store(ETaskState::CANCELED); }
    void setFailed() noexcept { m_taskState->store(ETaskState::FAILED); }

  public:
    TaskID(const TaskID&)                     = default;
    TaskID(TaskID&&)                          = default;
    auto operator= (const TaskID&) -> TaskID& = default;
    auto operator= (TaskID&&) -> TaskID&      = default;
    ~TaskID()                                 = default;

    [[nodiscard]]
    explicit operator std::uint32_t () const noexcept
    {
        return m_taskId;
    }

    [[nodiscard]]
    auto getState() const noexcept -> ETaskState
    {
        return m_taskState->load();
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

template <>
struct std::hash<ThreadPool::TaskID>
{
    [[nodiscard]]
    auto operator() (const ThreadPool::TaskID& taskID) const noexcept -> std::size_t
    {
        return std::hash<std::uint32_t> {}(static_cast<std::uint32_t>(taskID));
    }
};

template <>
struct std::formatter<ThreadPool::TaskID> : public std::formatter<std::uint32_t>
{
    auto format(const ThreadPool::TaskID& taskID, std::format_context& ctx) const
    {
        return std::formatter<std::uint32_t>::format(static_cast<std::uint32_t>(taskID), ctx);
    }
};
