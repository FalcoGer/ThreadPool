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
#include <set>
#include <stack>
#include <stop_token>
#include <thread>
#include <vector>

export module ThreadPool;

export import :ETaskState;
export import :TaskCanceled;
export import :TaskID;
export import :TaskPriority;
export import :TaskTicket;

#ifndef __clang__
#  warning "Exporting internal module partitions because gcc errors otherwise"
export import :ITask;
export import :Task;
export import :ITaskUniquePtrPriorityComparator;
#else
import :ITask;
import :Task;
import :ITaskUniquePtrPriorityComparator;
#endif

namespace ThreadPool
{
export template <typename PromiseType = std::any>
class ThreadPool
{
  private:
    using ITaskPtrType           = std::unique_ptr<InternalDetail::ITask<PromiseType>>;
    using ITaskPtrTypeComparator = InternalDetail::ITaskUniquePtrPriorityComparator<PromiseType>;

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
                task = std::move(const_cast<ITaskPtrType&>(m_taskQueue.top()));
                m_taskQueue.pop();
            }
            if (task->getState() == ETaskState::WAITING)
            {
                task->run();
            }

            bool                              anyDependentTaskNeedsToBeRemoved {false};

            const std::lock_guard<std::mutex> DEPENDENCY_LOCK {m_tasksWithDependenciesMutex};
            // stack is used to recursively cancel tasks
            if (task->getState() == ETaskState::CANCELED || task->getState() == ETaskState::FAILED)
            {
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
                            anyDependentTaskNeedsToBeRemoved = true;
                        }
                    }
                }
            }
            else
            {
                // task was successful.
                for (auto& taskWithDependencies : m_tasksWithDependencies)
                {
                    if (taskWithDependencies->getState() == ETaskState::CANCELED
                        || taskWithDependencies->getState() == ETaskState::FAILED
                        || taskWithDependencies->getState() == ETaskState::FINISHED)
                    {
                        anyDependentTaskNeedsToBeRemoved = true;
                        continue;
                    }

                    // update the dependent task's dependency set
                    // if there are no more dependencies or the task was canceled, we need to update.
                    anyDependentTaskNeedsToBeRemoved = taskWithDependencies->updateDependency(task->getTaskID())
                                                         ? true
                                                         : anyDependentTaskNeedsToBeRemoved;

                    if (anyDependentTaskNeedsToBeRemoved && taskWithDependencies->getState() == ETaskState::WAITING)
                    {
                        // task dependencies were all fulfilled without having been canceled
                        const std::lock_guard<std::mutex> LOCK(m_queueMutex);
                        // the move leaves nullptr in m_tasksWithDependencies, will be cleaned up later
                        m_taskQueue.push(std::move(taskWithDependencies));
                        m_cvTaskReady.notify_one();
                    }
                }
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
