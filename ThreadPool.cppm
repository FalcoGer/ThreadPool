module;

#include <algorithm>
#include <any>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stack>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

export module ThreadPool;

export import :TaskCanceled;
export import :TaskID;
export import :TaskPriority;
export import :TaskTicket;
import :ITask;
import :Task;

namespace ThreadPool
{
export template <typename PromiseType>
class TaskTicket;

export template <typename PromiseType = std::any>
class ThreadPool
{
  public:
    explicit ThreadPool(const std::size_t NUM_THREADS = std::thread::hardware_concurrency()) { resize(NUM_THREADS); }

    void shutdownAndWait() noexcept
    {
        {
            const std::lock_guard<std::mutex> LOCK(m_queueMutex);
            for (auto& thread : m_threads)
            {
                thread.request_stop();
            }
        }
        m_cvTaskReady.notify_all();
        m_threads.clear();
    }

    void resize(const std::size_t NUM_THREADS)
    {
        if (NUM_THREADS > m_threads.size())
        {
            m_threads.reserve(NUM_THREADS);
            for ([[maybe_unused]]
                 auto threadNum : std::views::iota(m_threads.size(), NUM_THREADS))
            {
                m_threads.emplace_back([this](const std::stop_token& stop) { runner(stop); });
            }
        }
        else if (NUM_THREADS < m_threads.size())
        {
            std::for_each(
              std::next(m_threads.begin(), static_cast<ptrdiff_t>(NUM_THREADS)),
              m_threads.end(),
              [](auto& t) { t.request_stop(); }
            );
            m_cvTaskReady.notify_all();
            m_threads.erase(std::next(m_threads.begin(), static_cast<ptrdiff_t>(NUM_THREADS)), m_threads.end());
            m_threads.shrink_to_fit();
        }
        else
        {
            // empty
        }
    }

    [[nodiscard]]
    auto threadCount() const noexcept -> std::size_t
    {
        return m_threads.size();
    }

    ~ThreadPool() { shutdownAndWait(); }

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueue(CallableType&& callable, ArgTypes&&... args) -> TaskTicket<PromiseType>
    {
        return enqueueWithPriorityAndDependencies(
          TaskPriority {0}, std::set<TaskID> {}, std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...
        );
    }

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueueWithPriority(const TaskPriority PRIORITY, CallableType&& callable, ArgTypes&&... args)
      -> TaskTicket<PromiseType>
    {
        return enqueueWithPriorityAndDependencies(
          PRIORITY, std::set<TaskID> {}, std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...
        );
    }

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueueWithDependencies(
      std::convertible_to<std::set<TaskID>> auto&& dependencies, CallableType&& callable, ArgTypes&&... args
    ) -> TaskTicket<PromiseType>
    {
        return enqueueWithPriorityAndDependencies(
          TaskPriority {0},
          std::forward<decltype(dependencies)>(dependencies),
          std::forward<CallableType>(callable),
          std::forward<ArgTypes>(args)...
        );
    }

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueueWithPriorityAndDependencies(
      const TaskPriority                           PRIORITY,
      std::convertible_to<std::set<TaskID>> auto&& dependencies,
      CallableType&&                               callable,
      ArgTypes&&... args
    ) -> TaskTicket<PromiseType>
    {
        using ReturnType = std::invoke_result_t<CallableType, ArgTypes...>;

        auto task        = std::make_unique<InternalDetail::Task<ReturnType, PromiseType, ArgTypes...>>(
          TaskID(m_taskCounter++),
          PRIORITY,
          std::set<TaskID> {std::forward<decltype(dependencies)>(dependencies)},
          std::forward<CallableType>(callable),
          std::forward<ArgTypes>(args)...
        );

        return enqueue(std::move(task));
    }

    ThreadPool(const ThreadPool&)                     = delete;
    auto operator= (const ThreadPool&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&)                          = delete;
    auto operator= (ThreadPool&&) -> ThreadPool&      = delete;

  private:
    void runner(const std::stop_token& stop)
    {
        while (!stop.stop_requested())
        {
            // Fetch a task from the queue and run it.
            // task gets destroyed after running when this unique_ptr goes out of scope.
            std::unique_ptr<InternalDetail::ITask<PromiseType>> task;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cvTaskReady.wait(lock, [this, &stop] { return !m_taskQueue.empty() || stop.stop_requested(); });
                if (stop.stop_requested())
                {
                    break;
                }
                task = std::move(const_cast<std::unique_ptr<InternalDetail::ITask<PromiseType>>&>(m_taskQueue.top()));
                m_taskQueue.pop();
            }
            task->run();

            {
                // remove taskid from taskid list only after run is complete
                // so that new tasks can check if their dependencies are not yet fulfilled
                // by a currently running task
                const std::lock_guard<std::mutex> LOCK(m_queueMutex);
                m_taskQueueTaskIds.erase(task->getTaskID());
                if (task->getState() == InternalDetail::ITask<PromiseType>::ETaskState::FAILED)
                {
                    const std::lock_guard<std::mutex> FAILED_TASKS_LOCK {m_failedTasksMutex};
                    m_failedTasks[task->getTaskID()] = InternalDetail::ITask<PromiseType>::ETaskState::FAILED;
                }
            }

            updateDependentTasksOnTaskCompletion(task);
        }
    }

    void updateDependentTasksOnTaskCompletion(const std::unique_ptr<InternalDetail::ITask<PromiseType>>& task)
    {
        bool needToEraseFromDependentTaskQueue {false};
        // task is done, check if we have dependent tasks for the task.
        // if task successful, move to queue.
        // otherwise set future exception and drop the task.

        if (task->getState() == InternalDetail::ITask<PromiseType>::ETaskState::FINISHED)
        {
            const std::lock_guard<std::mutex> DEPENDENCY_LOCK {m_tasksWithDependenciesMutex};
            for (auto& taskWithDependencies : m_tasksWithDependencies)
            {
                if (!taskWithDependencies
                    || taskWithDependencies->getState() == InternalDetail::ITask<PromiseType>::ETaskState::CANCELED)
                {
                    needToEraseFromDependentTaskQueue = true;
                    continue;
                }
                if (taskWithDependencies->removeDependency(task->getTaskID()))
                {
                    // task has no more unfulfilled dependencies because another task finished
                    // task done successfully, move dependent task to task queue for execution
                    {
                        const TaskID                      TASK_ID = taskWithDependencies->getTaskID();
                        const std::lock_guard<std::mutex> LOCK(m_queueMutex);
                        m_taskQueue.push(
                          std::move(taskWithDependencies)
                        );    // leaves nullptr in vector, will be cleaned up later
                        m_taskQueueTaskIds.insert(TASK_ID);
                    }
                    m_cvTaskReady.notify_one();
                }
            }
        }
        else if (task->getState() == InternalDetail::ITask<PromiseType>::ETaskState::FAILED)
        {
            // need to recursively cancel all dependent tasks
            std::lock(m_tasksWithDependenciesMutex, m_failedTasksMutex);
            const std::lock_guard<std::mutex> DEPENDENCY_LOCK {m_tasksWithDependenciesMutex, std::adopt_lock};
            const std::lock_guard<std::mutex> FAILED_TASKS_LOCK {m_failedTasksMutex, std::adopt_lock};

            for (auto& taskWithDependencies : m_tasksWithDependencies)
            {
                if (!taskWithDependencies
                    || taskWithDependencies->getState() == InternalDetail::ITask<PromiseType>::ETaskState::CANCELED)
                {
                    needToEraseFromDependentTaskQueue = true;
                    continue;
                }

                if (taskWithDependencies->hasDependency(task->getTaskID()))
                {
                    // dependency failed, all depdendent tasks must be canceled.
                    std::stack<TaskID> canceledTasks {};
                    const TaskID       ORIGINAL_CANCELED_TASK {task->getTaskID()};
                    canceledTasks.push(ORIGINAL_CANCELED_TASK);

                    while (!canceledTasks.empty())
                    {
                        const TaskID CANCELED_TASK_ID {canceledTasks.top()};
                        canceledTasks.pop();
                        if (!m_failedTasks.contains(CANCELED_TASK_ID))
                        {
                            m_failedTasks[CANCELED_TASK_ID] = InternalDetail::ITask<PromiseType>::ETaskState::CANCELED;
                        }
                        for (auto& taskToCancel : m_tasksWithDependencies)
                        {
                            if (taskToCancel
                                && taskToCancel->getState() == InternalDetail::ITask<PromiseType>::ETaskState::WAITING
                                && taskToCancel->hasDependency(CANCELED_TASK_ID))
                            {
                                std::string reason {std::format(
                                  "Task {} canceled because task {} which this task depends on {}.",
                                  taskToCancel->getTaskID(),
                                  CANCELED_TASK_ID,
                                  m_failedTasks[CANCELED_TASK_ID]
                                      == InternalDetail::ITask<PromiseType>::ETaskState::FAILED
                                    ? "has failed"
                                    : "got canceled"
                                )};

                                taskToCancel->setCanceled(std::make_exception_ptr(TaskCanceled(reason)));
                                canceledTasks.push(taskToCancel->getTaskID());
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // this shouldn't happen
        }


        if (needToEraseFromDependentTaskQueue)
        {
            std::erase_if(
              m_tasksWithDependencies,
              [](const auto& taskWithDependencies)
              {
                  return !taskWithDependencies
                         || taskWithDependencies->getState() == InternalDetail::ITask<PromiseType>::ETaskState::CANCELED
                         || taskWithDependencies->getState() == InternalDetail::ITask<PromiseType>::ETaskState::FAILED
                         || taskWithDependencies->getState()
                              == InternalDetail::ITask<PromiseType>::ETaskState::FINISHED;
              }
            );
        }
    }

    auto enqueue(std::unique_ptr<InternalDetail::ITask<PromiseType>>&& task) -> TaskTicket<PromiseType>
    {
        const TaskID            TASK_ID = task->getTaskID();
        TaskTicket<PromiseType> ticket {TASK_ID, task->getFuture()};

        if (!task->hasDependencies())
        {
            {
                const std::lock_guard<std::mutex> LOCK(m_queueMutex);
                m_taskQueue.push(std::move(task));
                m_taskQueueTaskIds.insert(TASK_ID);
            }
            m_cvTaskReady.notify_one();
        }
        else
        {
            // check if task has any dependencies that have failed already
            const std::lock_guard<std::mutex> FAILED_TASKS_LOCK {m_failedTasksMutex};
            for (const auto [FAILED_TASK_ID, FAILED_TASK_STATE] : m_failedTasks)
            {
                if (task->hasDependency(FAILED_TASK_ID))
                {
                    std::string reason {std::format(
                      "Task {} canceled because task {} which this task depends on {}.",
                      task->getTaskID(),
                      FAILED_TASK_ID,
                      FAILED_TASK_STATE == InternalDetail::ITask<PromiseType>::ETaskState::FAILED
                        ? "has failed"
                        : "got canceled"
                    )};
                    task->setCanceled(std::make_exception_ptr(TaskCanceled(reason)));
                    break;
                }
            }
            if (task->getState() == InternalDetail::ITask<PromiseType>::ETaskState::CANCELED)
            {
                m_failedTasks[TASK_ID] = InternalDetail::ITask<PromiseType>::ETaskState::CANCELED;
                return ticket;    // do not add task to any queue
            }

            // prevent new tasks from executing while we figure out whether this task's dependencies have already been fulfilled
            std::lock(m_queueMutex, m_tasksWithDependenciesMutex);
            const std::lock_guard<std::mutex> TASK_QUEUE_LOCK(m_queueMutex, std::adopt_lock);
            const std::lock_guard<std::mutex> DEPENDENCIES_QUEUE_LOCK(m_tasksWithDependenciesMutex, std::adopt_lock);
            {
                // remove any taskIDs from the dependency queue that are not in the task queue or dependency queue
                std::set<TaskID> taskIdsInQueue {m_taskQueueTaskIds};
                taskIdsInQueue.insert_range(
                  m_tasksWithDependencies | std::views::transform([](const auto& x) { return x->getTaskID(); })
                );

                task->removeAllDependenciesNotIn(std::move(taskIdsInQueue));
            }

            if (task->hasDependencies())
            {
                m_tasksWithDependencies.push_back(std::move(task));
            }
            else
            {
                m_taskQueue.push(std::move(task));
                m_taskQueueTaskIds.insert(TASK_ID);
                m_cvTaskReady.notify_one();
            }
        }
        return ticket;
    }

    std::uint32_t m_taskCounter {};
    std::priority_queue<
      std::unique_ptr<InternalDetail::ITask<PromiseType>>,
      std::vector<std::unique_ptr<InternalDetail::ITask<PromiseType>>>,
      InternalDetail::ITaskUniquePtrPriorityComparitor<PromiseType>>
                                                                              m_taskQueue;
    std::set<TaskID>                                                          m_taskQueueTaskIds;
    std::map<TaskID, typename InternalDetail::ITask<PromiseType>::ETaskState> m_failedTasks;
    std::vector<std::unique_ptr<InternalDetail::ITask<PromiseType>>>          m_tasksWithDependencies;
    std::vector<std::jthread>                                                 m_threads;
    std::mutex                                                                m_queueMutex;
    std::mutex                                                                m_tasksWithDependenciesMutex;
    std::mutex                                                                m_failedTasksMutex;
    std::condition_variable                                                   m_cvTaskReady;
};
}    // namespace ThreadPool
