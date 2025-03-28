module;

#include <algorithm>
#include <any>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <set>
#include <stack>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

export module ThreadPool;

export import :ETaskState;
export import :TaskCanceled;
export import :TaskID;
export import :TaskPriority;
export import :TaskTicket;

export import :ITask;
export import :Task;
export import :ITaskUniquePtrPriorityComparator;

namespace ThreadPool
{
/// @class ThreadPool
/// @brief A thread pool for executing tasks.
///
/// This class implements a thread pool for executing tasks.
/// Tasks are executed in the order of their priority.
/// @tparam PromiseType type of the result of the tasks
/// @note Objects of this type can not be moved or copied.
export template <typename PromiseType = std::any>
class ThreadPool
{
  private:
    using ITaskPtrType           = std::unique_ptr<InternalDetail::ITask<PromiseType>>;
    using ITaskPtrTypeComparator = InternalDetail::ITaskUniquePtrPriorityComparator<PromiseType>;

  public:
    /// @brief Constructs a ThreadPool with the specified number of threads.
    ///
    /// @param NUM_THREADS number of threads in the thread pool.
    /// The default value is std::thread::hardware_concurrency()
    explicit ThreadPool(const std::size_t NUM_THREADS = std::thread::hardware_concurrency()) { resize(NUM_THREADS); }

    /// @brief Stop all threads and block until all tasks have finished.
    ///
    /// This function requests all threads to stop and then blocks until all tasks have finished.
    /// The thread pool is empty after this call.
    /// @note This function does not throw and is marked with @c noexcept
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

    /// @brief Adjusts the number of threads in the thread pool.
    ///
    /// This function increases or decreases the number of threads in the thread pool
    /// based on the specified `NUM_THREADS`. If `NUM_THREADS` is greater than the
    /// current number of threads, new threads are added. If `NUM_THREADS` is less,
    /// excess threads are requested to stop and subsequently removed. If `NUM_THREADS`
    /// is equal to the current number of threads, no action is taken.
    ///
    /// @param NUM_THREADS The desired number of threads in the thread pool.
    /// @note If removed threads are running tasks, this will block until they are done.
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

    /// @brief Get the number of threads in the thread pool.
    ///
    /// This function returns the current number of threads active in the thread pool.
    ///
    /// @return The number of threads in the thread pool.
    /// @note This function does not throw and is marked with @c noexcept.

    [[nodiscard]]
    auto threadCount() const noexcept -> std::size_t
    {
        const std::lock_guard LOCK(m_queueMutex);
        return m_threads.size();
    }

    ~ThreadPool() { shutdownAndWait(); }

    /// @brief Enqueue a task to be executed.
    ///
    /// This function enqueues a task to be executed in the thread pool.
    /// The task is queued with a priority of 0 and no dependencies.
    ///
    /// @param callable Callable object to be executed.
    /// @param args Arguments to be passed to the callable.
    ///
    /// @return A @ref TaskTicket that can be used to retrieve the result of the task.
    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueue(CallableType&& callable, ArgTypes&&... args) -> TaskTicket<PromiseType>
    {
        return enqueueWithPriorityAndDependencies(
          TaskPriority {0}, std::set<TaskID> {}, std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...
        );
    }

    /// @brief Enqueue a task with a specified priority to be executed.
    ///
    /// This function enqueues a task to be executed in the thread pool with a given priority.
    /// The task is queued without any dependencies.
    ///
    /// @param PRIORITY The priority of the task. Higher values indicate higher priority.
    /// @param callable Callable object to be executed.
    /// @param args Arguments to be passed to the callable.
    ///
    /// @return A @ref TaskTicket that can be used to retrieve the result of the task.

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

    /// @brief Enqueue a task with dependencies to be executed.
    ///
    /// This function enqueues a task to be executed in the thread pool with a given set of dependencies.
    /// The task is queued with a priority of 0.
    ///
    /// @param dependencies A set of task IDs that must be finished before the task can be executed.
    /// @param callable Callable object to be executed.
    /// @param args Arguments to be passed to the callable.
    ///
    /// @return A @ref TaskTicket that can be used to retrieve the result of the task.
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

    /// @brief Enqueue a task with a given priority and dependencies to be executed.
    ///
    /// This function enqueues a task to be executed in the thread pool with a given set of dependencies.
    /// The task is queued with the given priority.
    ///
    /// @param PRIORITY The priority with which the task should be executed.
    /// @param dependencies A set of task IDs that must be finished before the task can be executed.
    /// @param callable Callable object to be executed.
    /// @param args Arguments to be passed to the callable.
    ///
    /// @return A @ref TaskTicket that can be used to retrieve the result of the task.
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
        using TaskType   = InternalDetail::Task<ReturnType, PromiseType, ArgTypes...>;

        // prevent new tasks from executing while we figure out whether this task's dependencies have already been
        // fulfilled this is done in the new task's constructor
        // locks are kept until the task is in either the task queue or the tasks with dependencies queue
        // or canceled.
        std::lock(m_queueMutex, m_tasksWithDependenciesMutex);
        const std::lock_guard<std::mutex> TASK_QUEUE_LOCK(m_queueMutex, std::adopt_lock);
        const std::lock_guard<std::mutex> DEPENDENCIES_QUEUE_LOCK(m_tasksWithDependenciesMutex, std::adopt_lock);

        auto                              task = std::make_unique<TaskType>(
          m_taskCounter++,
          PRIORITY,
          std::set<TaskID> {std::forward<decltype(dependencies)>(dependencies)},
          std::forward<CallableType>(callable),
          std::forward<ArgTypes>(args)...
        );

        auto ticket = task->makeTicket();

        if (task->getState() == ETaskState::CANCELED || task->getState() == ETaskState::FAILED)
        {
            // do not add task to any queue
            return ticket;
        }

        if (!task->hasDependencies())
        {
            m_taskQueue.push(std::move(task));
            m_cvTaskReady.notify_one();
        }
        else
        {
            m_tasksWithDependencies.push_back(std::move(task));
        }
        return ticket;
    }

    ThreadPool(const ThreadPool&)                     = delete;
    auto operator= (const ThreadPool&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&)                          = delete;
    auto operator= (ThreadPool&&) -> ThreadPool&      = delete;

  private:
    /// @brief This function is called by each thread in the thread pool.
    ///
    /// It runs until the thread pool is stopped (i.e., the stop token is triggered).
    /// The function waits until a task is available, then runs it.
    ///
    /// @param stop A stop token that can be used to stop this runner from getting more tasks.
    void runner(const std::stop_token& stop)
    {
        while (!stop.stop_requested())
        {
            // Fetch a task from the queue and run it.
            // task gets destroyed after running when this unique_ptr goes out of scope.
            ITaskPtrType task;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cvTaskReady.wait(lock, [this, &stop] { return !m_taskQueue.empty() || stop.stop_requested(); });
                if (stop.stop_requested())
                {
                    break;
                }
                // priority queue .top() returns only a const reference
                // this is because of the max heap invariant of the priority queue
                // changing any element could invalidate this invariant
                // but we need to move from the top of the queue since ITaskPtrType is non copyable
                // This leaves a nullptr in the priority queue, which we pop immediately.
                // Since the queue is locked through the mutex, this should be safe.
                task = std::move(const_cast<ITaskPtrType&>(m_taskQueue.top()));
                m_taskQueue.pop();
            }
            if (task->getState() == ETaskState::PENDING)
            {
                task->run();
            }

            updateDependentTasksQueue(task);
        }
    }

    /// @brief Updates the m_tasksWithDependencies vector based on the given task.
    ///
    /// If the task is finished, tasks that depend on the task are updated.
    /// If the task is canceled or failed, all dependent tasks are canceled as well.
    /// Tasks that are finished, canceled, or failed are removed from m_tasksWithDependencies.
    void updateDependentTasksQueue(const ITaskPtrType& task)
    {
        bool                              anyDependentTaskNeedsToBeRemoved {false};

        const std::lock_guard<std::mutex> DEPENDENCY_LOCK {m_tasksWithDependenciesMutex};
        // stack is used to recursively cancel tasks
        if (task->getState() == ETaskState::CANCELED || task->getState() == ETaskState::FAILED)
        {
            anyDependentTaskNeedsToBeRemoved = updateDependentTasksQueueForFailedTask(task);
        }
        else
        {
            anyDependentTaskNeedsToBeRemoved = updateDependentTasksQueueForFinishedTask(task);
        }

        if (anyDependentTaskNeedsToBeRemoved)
        {
            std::erase_if(
              m_tasksWithDependencies,
              [](const auto& taskWithDependencies)
              {
                  // clean up the nullptr we left when moving to the task queue.
                  return !taskWithDependencies
                         || taskWithDependencies->getState() == ETaskState::CANCELED
                         || taskWithDependencies->getState() == ETaskState::FAILED
                         || taskWithDependencies->getState() == ETaskState::FINISHED;
              }
            );
        }
    }

    /// @brief Updates the m_tasksWithDependencies vector when a task is finished.
    ///
    /// Goes through all dependent tasks and updates their state.
    /// If a tasks dependencies are fulfilled it is moved to the task queue for execution.
    /// @returns true if the dependen tasks vector need updating.
    auto updateDependentTasksQueueForFinishedTask(const ITaskPtrType& task) -> bool
    {
        bool result {false};
        for (auto& taskWithDependencies : m_tasksWithDependencies)
        {
            if (taskWithDependencies->getState() == ETaskState::CANCELED
                || taskWithDependencies->getState() == ETaskState::FAILED
                || taskWithDependencies->getState() == ETaskState::FINISHED)
            {
                result = true;
                continue;
            }

            // update the dependent task's dependency set
            // if there are no more dependencies or the task was canceled, we need to update.

            if (taskWithDependencies->updateDependency(task->getTaskID())
                && taskWithDependencies->getState() == ETaskState::PENDING)
            {
                result = true;
                // task dependencies were all fulfilled without having been canceled
                {
                    const std::lock_guard<std::mutex> LOCK(m_queueMutex);
                    // the move leaves nullptr in m_tasksWithDependencies, will be cleaned up later
                    m_taskQueue.push(std::move(taskWithDependencies));
                }
                m_cvTaskReady.notify_one();
            }
        }
        return result;
    }


    /// @brief Updates the m_tasksWithDependencies vector when a task failed.
    ///
    /// Goes through all dependent tasks and cancels them recursively
    /// if they depend on the task that failed.
    /// @returns true if the dependen tasks vector need updating.
    auto updateDependentTasksQueueForFailedTask(const ITaskPtrType& task) -> bool
    {
        bool               result {false};
        std::stack<TaskID> canceledTasks {};
        canceledTasks.push(task->getTaskID());
        while (!canceledTasks.empty())
        {
            TaskID canceledTask = std::move(canceledTasks.top());
            canceledTasks.pop();

            for (auto& taskWithDependencies : m_tasksWithDependencies)
            {
                if (taskWithDependencies->updateDependency(canceledTask))
                {
                    // task depended on the canceledTask and was canceled in turn.
                    canceledTasks.push(taskWithDependencies->getTaskID());
                    result = true;
                }
            }
        }
        return result;
    }

    std::priority_queue<ITaskPtrType, std::vector<ITaskPtrType>, ITaskPtrTypeComparator> m_taskQueue;
    std::vector<ITaskPtrType>                                                            m_tasksWithDependencies;
    std::vector<std::jthread>                                                            m_threads;
    std::mutex                                                                           m_queueMutex;
    std::mutex                                                                           m_tasksWithDependenciesMutex;
    std::condition_variable                                                              m_cvTaskReady;
    std::uint32_t                                                                        m_taskCounter {};
};
}    // namespace ThreadPool
