module;

#include <atomic>
#include <exception>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>

export module ThreadPool:ITask;

import :TaskID;
import :TaskPriority;
import :ETaskState;
import :TaskTicket;
import :TaskCanceled;

namespace ThreadPool::InternalDetail
{
// no export, this is internal

/// @class ITask
/// @brief Interface for tasks that can be executed in the thread pool.
///
/// This class provides the interface for tasks that can be executed in the thread pool.
/// All tasks must implement this interface.
///
/// @tparam PromiseType type of the result of the task
template <typename PromiseType>
class ITask : protected TaskID
{
  private:
    std::promise<PromiseType>         m_promise;
    TaskPriority                      m_priority;
    std::set<TaskID>                  m_dependencies;
    mutable std::mutex                m_dependenciesMutex;
    std::optional<std::exception_ptr> m_exception;

    static constexpr const char* REASON_FMT {"Task {0} canceled, because task {1}, which task {0} depends on, {2}."};

  protected:
    /// @brief Get the promise associated with the task.
    /// @return A reference to the task's promise object, which can be used to set the result of the task.
    [[nodiscard]]
    auto getPromise() noexcept -> std::promise<PromiseType>&
    {
        return m_promise;
    }

    /// @brief Set the exception associated with the task.
    ///
    /// This method sets the exception associated with the task and stores it in the promise object.
    /// The exception is also stored in the @ref m_exception member variable.
    ///
    /// @param exception A pointer to the exception to be associated with the task.
    void setException(std::exception_ptr&& exception)
    {
        m_promise.set_exception(exception);
        m_exception = std::move(exception);
    }

    /// @brief Constructor for ITask.
    ///
    /// This constructor initializes the task's task ID, priority, and dependencies.
    /// The task's state is set to PENDING.
    ///
    /// @param TASK_ID The task ID for the newly constructed task.
    /// @param PRIORITY The priority for the newly constructed task.
    /// @param dependencies The dependencies for the newly constructed task.
    explicit ITask(const std::uint32_t TASK_ID, const TaskPriority PRIORITY, std::ranges::range auto&& dependencies)
        requires std::convertible_to<decltype(dependencies), decltype(m_dependencies)>
            : TaskID {TASK_ID, std::make_shared<std::atomic<ETaskState>>(ETaskState::PENDING)},
              m_priority(PRIORITY),
              m_dependencies(std::forward<decltype(dependencies)>(dependencies))
    {
        updateDependencies();
    }

    /// @brief Constructor for ITask.
    ///
    /// This constructor initializes the task's task ID, priority, and dependencies.
    /// The task's state is set to PENDING.
    ///
    /// @param TASK_ID The task ID for the newly constructed task.
    /// @param PRIORITY The priority for the newly constructed task.
    /// @param dependencies The dependencies for the newly constructed task.
    explicit ITask(const std::uint32_t TASK_ID, const TaskPriority PRIORITY, std::ranges::range auto&& dependencies)
        requires (!std::convertible_to<decltype(dependencies), decltype(m_dependencies)>)
            : TaskID {TASK_ID, std::make_shared<std::atomic<ETaskState>>(ETaskState::PENDING)},
              m_priority(PRIORITY),
              m_dependencies(std::ranges::begin(dependencies), std::ranges::end(dependencies))
    {
        updateDependencies();
    }

    /// @brief Set the task as canceled and store the given exception.
    ///
    /// This method sets the task's state to CANCELED and stores the given exception in the promise object.
    /// The exception is also stored in the @ref m_exception member variable.
    void setCanceled(std::exception_ptr&& exception)
    {
        TaskID::setCanceled();
        setException(std::move(exception));
    }

    /// @brief Set the task as failed and store the given exception.
    ///
    /// This method sets the task's state to FAILED and stores the given exception in the promise object.
    /// The exception is also stored in the @ref m_exception member variable.
    ///
    /// @param exception A pointer to the exception to be associated with the task.
    void setFailed(std::exception_ptr&& exception)
    {
        TaskID::setFailed();
        setException(std::move(exception));
    }

    /// @brief Check if a dependent task has been canceled or failed, and cancel the current task if so.
    ///
    /// This method checks the state of a dependent task and cancels the current task if the dependent task
    /// is either canceled or failed. If the dependent task is canceled, the current task is canceled with
    /// an appropriate exception message indicating the cancellation reason. Similarly, if the dependent task
    /// has failed, the current task is canceled with an exception message indicating the failure reason.
    ///
    /// @param taskId The ID of the dependent task to check.
    /// @return True if the current task was canceled due to the dependent task's state; false otherwise.
    ///
    /// @pre The @p taskId must be present in the current task's dependencies (@c m_dependencies).
    [[nodiscard]]
    auto cancelIfTaskCanceled(const TaskID& taskId) -> bool
    {
        if (taskId.getState() == ETaskState::CANCELED)
        {
            setCanceled(
              std::make_exception_ptr(
                TaskCanceled(std::format(REASON_FMT, static_cast<TaskID>(*this), taskId, "got canceled"))
              )
            );
            return true;
        }
        if (taskId.getState() == ETaskState::FAILED)
        {
            setCanceled(
              std::make_exception_ptr(
                TaskCanceled(std::format(REASON_FMT, static_cast<TaskID>(*this), taskId, "has failed"))
              )
            );
            return true;
        }
        return false;
    }

  public:
    virtual ~ITask()   = default;

    /// @brief Execute the task.
    ///
    /// This pure virtual function is intended to be overridden by derived classes
    /// to define the specific behavior of the task when executed. Implementations
    /// should ensure that the task's state is appropriately managed, transitioning
    /// from PENDING to RUNNING, and then to either FINISHED, FAILED, or CANCELED
    /// based on the execution outcome.
    virtual void run() = 0;

    /// @brief Get the state of the task.
    /// @return The state of the task.
    [[nodiscard]]
    auto getState() const noexcept -> ETaskState
    {
        return TaskID::getState();
    }

    /// @brief Create a ticket for the task.
    /// @return A `TaskTicket<PromiseType>` that can be used to wait for the task to finish.
    ///         The ticket contains a copy of the task's ID and a `std::future<PromiseType>` that
    ///         can be used to retrieve the result of the task.
    [[nodiscard]]
    auto makeTicket() -> TaskTicket<PromiseType>
    {
        return TaskTicket<PromiseType> {getTaskID(), m_promise.get_future()};
    }

    /// @brief Check if the task has any dependencies.
    /// @return `true` if the task has any dependencies, `false` otherwise.
    ///         This method is thread-safe.
    [[nodiscard]]
    auto hasDependencies() const -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return !m_dependencies.empty();
    }

    /// @brief Check if the task depends on the given task id.
    ///
    /// This method checks if the task depends on the given task id.
    /// The check is thread-safe.
    /// @param taskId The task id to check for.
    /// @return `true` if the task depends on the given task id, `false` otherwise.
    [[nodiscard]]
    auto hasDependency(const TaskID& taskId) -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return m_dependencies.contains(taskId);
    }

    /// @brief Update the dependency set of the task for the given task id.
    ///        If the task is canceled, the task is canceled as well.
    ///        If the task is finished, it is removed from the dependency set.
    /// @param taskId The id of the task to update the dependency set for.
    /// @return If the task was canceled, or if the dependency set is empty.
    auto updateDependency(const TaskID& taskId) -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        if (!m_dependencies.contains(taskId))
        {
            return false;
        }
        if (cancelIfTaskCanceled(taskId))
        {
            return true;
        }

        // task wasn't canceled.
        if (taskId.getState() == ETaskState::FINISHED)
        {
            // remove finished dependency
            m_dependencies.erase(taskId);
        }
        return m_dependencies.empty();
    }

    /// @brief Update the dependency set of the task by removing any finished dependencies.
    ///        If any dependency is canceled, the task is canceled as well.
    /// @return If the task was canceled, or if the dependency set is empty.
    auto updateDependencies() -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        std::erase_if(m_dependencies, [](const auto& taskId) { return taskId.getState() == ETaskState::FINISHED; });

        for (const auto& taskId : m_dependencies)
        {
            bool result = cancelIfTaskCanceled(taskId);
            if (result)
            {
                return true;
            }
        }
        return m_dependencies.empty();
    }

    /// @brief Get the task id of the task.
    ///
    /// This function returns the task id of the task.
    /// A copy of the task id is created and returned.
    /// This function is thread-safe.
    /// @returns The task id of the task.
    ///
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto getTaskID() const noexcept -> TaskID
    {
        return TaskID(*this);    // make a copy
    }

    /// @brief Get the exception associated with the task, if any.
    ///
    /// This method returns the exception associated with the task, if any.
    /// If the task has not been canceled or failed, this method returns an empty optional.
    /// A copy of the stored exception is returned.
    /// @returns The exception associated with the task, if any.
    ///
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto getException() const noexcept -> std::optional<std::exception_ptr>
    {
        return m_exception;
    }

    /// @brief Get the priority associated with the task.
    ///
    /// This method returns the priority associated with the task.
    /// A copy of the stored priority is returned.
    /// @returns The priority associated with the task.
    ///
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto getPriority() const noexcept -> TaskPriority
    {
        return m_priority;
    }
};

}    // namespace ThreadPool::InternalDetail
