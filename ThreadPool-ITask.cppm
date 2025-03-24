module;

#include <atomic>
#include <exception>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <set>

export module ThreadPool:ITask;

import :TaskID;
import :TaskPriority;
import :ETaskState;
import :TaskTicket;
import :TaskCanceled;

namespace ThreadPool::InternalDetail
{
export template <typename PromiseType>
class ITask : public TaskID
{
  private:
    std::promise<PromiseType>         m_promise;
    TaskPriority                      m_priority;
    std::set<TaskID>                  m_dependencies;
    mutable std::mutex                m_dependenciesMutex;
    std::optional<std::exception_ptr> m_exception;

    static constexpr const char* REASON_FMT {"Task {0} canceled, because task {1}, which task {0} depends on, {2}."};

  protected:
    [[nodiscard]]
    auto getPromise() noexcept -> std::promise<PromiseType>&
    {
        return m_promise;
    }

    void setException(std::exception_ptr&& exception) noexcept
    {
        m_promise.set_exception(exception);
        m_exception = std::move(exception);
    }

    explicit ITask(const std::uint32_t TASK_ID, const TaskPriority PRIORITY, std::set<TaskID>&& dependencies)
            : TaskID {TASK_ID, std::make_shared<std::atomic<ETaskState>>(ETaskState::WAITING)},
              m_priority(PRIORITY),
              m_dependencies(std::move(dependencies))
    {
        updateDependencies();
    }

    void setCanceled(std::exception_ptr&& exception) noexcept
    {
        TaskID::setCanceled();
        setException(std::move(exception));
    }

    void setFailed(std::exception_ptr&& exception) noexcept
    {
        TaskID::setFailed();
        setException(std::move(exception));
    }

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

    virtual void run() = 0;

    [[nodiscard]]
    auto getResult() -> PromiseType
    {
        return m_promise.get_future().get();
    }

    [[nodiscard]]
    auto getFuture() noexcept -> std::future<PromiseType>
    {
        return m_promise.get_future();
    }

    [[nodiscard]]
    auto makeTicket() noexcept -> TaskTicket<PromiseType>
    {
        return TaskTicket<PromiseType> {getTaskID(), getFuture()};
    }

    [[nodiscard]]
    auto hasDependencies() const noexcept -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return !m_dependencies.empty();
    }

    [[nodiscard]]
    auto hasDependency(const TaskID& taskId) -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return m_dependencies.contains(taskId);
    }

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

    [[nodiscard]]
    auto getTaskID() const noexcept -> TaskID
    {
        return TaskID(*this);
    }

    [[nodiscard]]
    auto getException() const noexcept -> std::optional<std::exception_ptr>
    {
        return m_exception;
    }

    [[nodiscard]]
    auto getPriority() const noexcept -> TaskPriority
    {
        return m_priority;
    }
};

}    // namespace ThreadPool::InternalDetail
