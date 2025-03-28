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
/// @class TaskID
/// @brief A unique identifier for a task, and a shared pointer to the ETaskState of the task.
///
/// This class is used to identify tasks in the ThreadPool.
/// It contains a unique std::uint32_t identifier and a shared pointer to the ETaskState of the task.
export class TaskID
{
  private:
    std::uint32_t                            m_taskId;
    // the state of the task is part of the task as well as the ticket
    // either can outlive the other
    std::shared_ptr<std::atomic<ETaskState>> m_taskState;

  protected:
    /// @brief Constructs a TaskID with the given ID and task state.
    ///
    /// This constructor initializes a TaskID with a unique identifier and a shared pointer
    /// to the atomic task state. The task state must not be nullptr.
    ///
    /// @param id The unique identifier for the task.
    /// @param taskState A shared pointer to the atomic state of the task.
    /// @throws std::invalid_argument if taskState is nullptr.
    ///
    /// @note TaskID can not exist on it's own and thus it's constructor is protected
    explicit TaskID(std::uint32_t id, std::shared_ptr<std::atomic<ETaskState>> taskState)
            : m_taskId(id), m_taskState(std::move(taskState))
    {
        if (m_taskState == nullptr)
        {
            throw std::invalid_argument("taskState shared ptr was null");
        }
    }

    /// @brief Set the state of the task to RUNNING.
    /// @note This does not throw and is marked with @c noexcept
    void setRunning() noexcept { m_taskState->store(ETaskState::RUNNING); }
    /// @brief Set the state of the task to FINISHED.
    /// @note This does not throw and is marked with @c noexcept
    void setFinished() noexcept { m_taskState->store(ETaskState::FINISHED); }
    /// @brief Set the state of the task to CANCELED.
    /// @note This does not throw and is marked with @c noexcept
    void setCanceled() noexcept { m_taskState->store(ETaskState::CANCELED); }
    /// @brief Set the state of the task to FAILED.
    /// @note This does not throw and is marked with @c noexcept
    void setFailed() noexcept { m_taskState->store(ETaskState::FAILED); }

  public:
    TaskID(const TaskID&)                     = default;
    TaskID(TaskID&&)                          = default;
    auto operator= (const TaskID&) -> TaskID& = default;
    auto operator= (TaskID&&) -> TaskID&      = default;
    ~TaskID()                                 = default;

    /// @brief Conversion operator to underlying std::uint32_t.
    ///
    /// @returns The underlying task id.
    /// @note This does not throw and is marked with @c noexcept
    [[nodiscard]]
    explicit operator std::uint32_t() const noexcept
    {
        return m_taskId;
    }

    /// @brief Get the state of the task.
    ///
    /// @returns The state of the task, as an ETaskState value.
    /// @note This does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto getState() const noexcept -> ETaskState
    {
        return m_taskState->load();
    }

    /// @brief Compares two TaskID objects.
    ///
    /// @param lhs left operand
    /// @param rhs right operand
    /// @returns A std::strong_ordering that indicates the result of the comparison.
    ///          The ordering is based on the underlying std::uint32_t values.
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    friend auto operator<=> (const TaskID& lhs, const TaskID& rhs) noexcept -> std::strong_ordering
    {
        return lhs.m_taskId <=> rhs.m_taskId;
    }

    /// @brief Compares two TaskID objects for equality.
    ///
    /// @param lhs left operand
    /// @param rhs right operand
    /// @returns true if the two TaskID objects are equal, false otherwise.
    ///          Two TaskID objects are equal if and only if their underlying
    ///          std::uint32_t values are equal.
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    friend auto operator== (const TaskID& lhs, const TaskID& rhs) noexcept -> bool
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
