module;

#include <algorithm>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <set>

export module ThreadPool:ITask;

import :TaskID;
import :TaskPriority;

namespace ThreadPool::InternalDetail
{
export template <typename PromiseType>
class ThreadPool;

export template <typename PromiseType>
class ITask
{
  public:
    enum class ETaskState : std::uint8_t
    {
        WAITING = 0,
        STARTED,
        FINISHED,
        FAILED,
        CANCELED
    };

  private:
    ETaskState                        m_state {ETaskState::WAITING};
    std::promise<PromiseType>         m_promise;
    TaskID                            m_taskId;
    TaskPriority                      m_priority;
    std::set<TaskID>                  m_dependencies;
    mutable std::mutex                m_dependenciesMutex;
    std::optional<std::exception_ptr> m_exception;

  protected:
    void setStarted() noexcept { m_state = ETaskState::STARTED; }
    void setFinished() noexcept { m_state = ETaskState::FINISHED; }

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

    explicit ITask(const TaskID TASK_ID, const TaskPriority PRIORITY, std::set<TaskID>&& dependencies)
            : m_taskId {TASK_ID}, m_priority(PRIORITY), m_dependencies(std::move(dependencies))
    {
        // empty
    }

  public:
    virtual ~ITask()   = default;

    virtual void run() = 0;

    void         setCanceled(std::exception_ptr&& exception) noexcept
    {
        m_state = ETaskState::CANCELED;
        setException(std::move(exception));
    }

    void setFailed(std::exception_ptr&& exception) noexcept
    {
        m_state = ETaskState::FAILED;
        setException(std::move(exception));
    }

    [[nodiscard]]
    auto getResult() -> PromiseType
    {
        return m_promise.get_future().get();
    }

    [[nodiscard]]
    auto getState() const noexcept -> ETaskState
    {
        return m_state;
    }

    [[nodiscard]]
    auto getFuture() noexcept -> std::future<PromiseType>
    {
        return m_promise.get_future();
    }

    [[nodiscard]]
    auto hasDependencies() const noexcept -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return !m_dependencies.empty();
    }

    [[nodiscard]]
    auto hasDependency(const TaskID TASK_ID) -> bool
    {
        const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
        return m_dependencies.contains(TASK_ID);
    }

    auto removeDependency(const TaskID TASK_ID) -> bool
    {
        {
            const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
            m_dependencies.erase(TASK_ID);
        }
        return !hasDependencies();
    }

    auto removeAllDependenciesNotIn(const std::set<TaskID>& taskIds) -> bool
    {
        std::set<TaskID> temp;
        {
            const std::lock_guard<std::mutex> DEPENDENCIES_LOCK(m_dependenciesMutex);
            std::ranges::set_intersection(m_dependencies, taskIds, std::inserter(temp, std::begin(temp)));
            m_dependencies = std::move(temp);
        }
        return !hasDependencies();
    }

    [[nodiscard]]
    auto getTaskID() const noexcept -> TaskID
    {
        return m_taskId;
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
