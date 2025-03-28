module;

#include <any>
#include <format>
#include <functional>
#include <future>
#include <stdexcept>
#include <utility>

export module ThreadPool:TaskTicket;

import :TaskID;
import :ETaskState;

namespace ThreadPool
{
namespace InternalDetail
{
    // forward declaration. not exported as it is internal
    template <typename PromiseType>
    class ITask;
}

/// @class TaskTicket
/// @brief A ticket to a task in the ThreadPool.
///
/// This class is returned by the ThreadPool::enqueue functions.
/// It it is used to retrieve the result and status of the task and
/// to make dependent tasks.
/// @tparam PromiseType type of the result of the task
export template <typename PromiseType = std::any>
class TaskTicket
{
  private:
    TaskID                   m_taskId;
    std::future<PromiseType> m_future;

    // only allow Tasks to create their own tickets
    // since Task classes are not exported,
    // users can not create their own fake tasktickets
    friend InternalDetail::ITask<PromiseType>;

    /// @brief Constructor for TaskTicket.
    ///
    /// @param taskId the TaskID of the task
    /// @param future the future of the task
    ///
    /// @throws std::invalid_argument if the future is not valid
    TaskTicket(TaskID taskId, std::future<PromiseType>&& future)
            : m_taskId(std::move(taskId)), m_future(std::move(future))
    {
        if (!m_future.valid())
        {
            throw std::invalid_argument("future was not valid");
        }
    }

  public:
    /// @brief Get the TaskID associated with this ticket.
    ///
    /// @returns The TaskID of the task associated with this ticket.
    [[nodiscard]]
    auto getTaskID() const noexcept -> const TaskID&
    {
        return m_taskId;
    }

    /// @brief Get the future associated with this ticket.
    ///
    /// @returns The future of the task associated with this ticket.
    /// @note Prefer using the .get() functions than to interact directly with the future.
    [[nodiscard]]
    auto getFuture() noexcept -> std::future<PromiseType>&
    {
        return m_future;
    }

    /// @brief Get the state of the task associated with this ticket.
    ///
    /// This method returns the current state of the task associated with the ticket.
    /// The state is represented as an ETaskState value and reflects whether the task
    /// is pending, running, finished, canceled, or failed.
    ///
    /// @returns The state of the task as an ETaskState value.
    /// @note This function does not throw and is marked with @c noexcept.
    [[nodiscard]]
    auto getState() const noexcept -> ETaskState
    {
        return m_taskId.getState();
    }

    /// @brief Check if the result of the task is ready.
    ///
    /// This method checks if the task associated with this ticket is ready.
    /// A task is ready if it has finished executing and the result is available.
    ///
    /// @returns `true` if the task is ready, `false` otherwise.
    /// @throws std::runtime_error if the future is not valid.
    /// @note This function is not thread safe as the future can become invalid after the check.
    [[nodiscard]]
    auto isReady() const -> bool
    {
        using std::chrono_literals::operator""ms;
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        // calling wait with future not valid is UB.
        return m_future.wait_for(0ms) == std::future_status::ready;
    }

    /// @brief Check if the ticket is valid.
    ///
    /// This method checks if the ticket is valid.
    /// A ticket is valid if the future associated with the ticket is valid.
    ///
    /// @returns `true` if the ticket is valid, `false` otherwise.
    /// @note This function does not throw and is marked with @c noexcept.
    [[nodiscard]]
    auto isValid() const noexcept -> bool
    {
        return m_future.valid();
    }

    /// @brief Get the result of the task.
    ///
    /// This method retrieves the result of the task associated with this ticket.
    /// The method blocks until the task is ready.
    /// If the task throws an exception, the exception is rethrown.
    ///
    /// @returns The result of the task.
    /// @throws @c std::runtime_error if the future is not valid.
    /// @throws Whatever the task throws.
    /// @note This function is not thread safe as the future can become invalid after the check.
    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> PromiseType
        requires (!std::is_void_v<PromiseType>)
    {
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        return m_future.get();
    }

    /// @brief Get the result of the task.
    ///
    /// This method retrieves the result of the task associated with this ticket.
    /// The method blocks until the task is ready.
    /// If the task throws an exception, the exception is rethrown.
    ///
    /// @throws @c std::runtime_error if the future is not valid.
    /// @throws Whatever the task throws.
    /// @note This overload is only available if the `PromiseType` is `void`.
    ///       No value is returned, and the result of the task is discarded.
    /// @note This function is not thread safe as the future can become invalid after the check.
    void get()
        requires std::is_void_v<PromiseType>
    {
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        m_future.get();
    }

    /// @brief Get the result of the task as type T.
    ///
    /// This overload is only available if the `PromiseType` is `std::any`.
    /// The method blocks until the task is ready.
    /// If the task throws an exception, the exception is rethrown.
    ///
    /// @returns The result of the task casted to `T`.
    ///
    /// @throws std::bad_any_cast if the cast to `T` fails.
    /// @throws @c std::runtime_error if the future is not valid.
    /// @throws Whatever the task throws.
    /// @note This function is not thread safe as the future can become invalid after the check.
    template <typename T>
        requires std::same_as<PromiseType, std::any> && (!std::same_as<T, std::any>) && (!std::is_void_v<T>)
    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> T
    {
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        // PromiseType is any, and T is not any or void.
        return std::any_cast<T>(m_future.get());
    }

    /// @brief Get the result of the task as type T.
    ///
    /// This method retrieves the result of the task associated with this ticket.
    /// The method blocks until the task is ready.
    /// If the task throws an exception, the exception is rethrown.
    ///
    /// @note This overload is only available if `PromiseType` is the same as `T`
    ///       and neither is `void`.
    ///
    /// @returns The result of the task as type `T`.
    ///
    /// @throws @c std::runtime_error if the future is not valid.
    /// @throws Whatever the task throws.
    /// @note This function is not thread safe as the future can become invalid after the check.
    template <typename T>
        requires std::same_as<PromiseType, T> && (!std::is_void_v<T>)
    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> T
    {
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        // T is the same as PromiseType, neither is void
        return m_future.get();
    }

    /// @brief Get the result of the task as type T.
    ///
    /// This overload is only available if `T` is `void`.
    /// The method blocks until the task is ready.
    /// If the task throws an exception, the exception is rethrown.
    ///
    /// @throws @c std::runtime_error if the future is not valid.
    /// @note This overload is only available if `T` is `void`.
    ///       No value is returned, and the result of the task is discarded.
    /// @note This function is not thread safe as the future can become invalid after the check.
    template <typename T>
        requires std::is_void_v<T>
    void get()
    {
        if (!m_future.valid())
        {
            throw std::runtime_error("Future not valid");
        }
        m_future.get();
    }
};

}    // namespace ThreadPool

template <typename PromiseType>
// NOLINTNEXTLINE(cert-dcl58-cpp) // specialization for std::hash is okay.
struct std::hash<ThreadPool::TaskTicket<PromiseType>>
{
    auto operator() (const ThreadPool::TaskTicket<PromiseType>& ticket) const -> std::size_t
    {
        return std::hash<ThreadPool::TaskID> {}(ticket.getTaskID());
    }
};

template <typename PromiseType>
// NOLINTNEXTLINE(cert-dcl58-cpp) // specialization for std::formatter is okay.
struct std::formatter<ThreadPool::TaskTicket<PromiseType>>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto           format(const ThreadPool::TaskTicket<PromiseType>& ticket, std::format_context& ctx) const
    {
        std::string status = "Retrieved";
        if (ticket.isValid())
        {
            switch (ticket.getState())
            {
                case ThreadPool::ETaskState::PENDING:  status = "Pending"; break;
                case ThreadPool::ETaskState::RUNNING:  status = "Running"; break;
                case ThreadPool::ETaskState::FINISHED: status = "Finished"; break;
                case ThreadPool::ETaskState::FAILED:   status = "Failed"; break;
                case ThreadPool::ETaskState::CANCELED: status = "Canceled"; break;
            }
        }
        return std::format_to(ctx.out(), "Task {} ({})", ticket.getTaskID(), status);
    }
};
