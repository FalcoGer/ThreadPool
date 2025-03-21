module;

#include <algorithm>
#include <any>
#include <condition_variable>
#include <future>
#include <iterator>
#include <memory>
#include <queue>
#include <ranges>
#include <stop_token>
#include <thread>
#include <vector>

export module ThreadPool;

export import :ITask;
export import :Task;

export namespace ThreadPool
{
template <typename PromiseType = std::any>
class ThreadPool
{
  public:
    explicit ThreadPool(const std::size_t NUM_THREADS = std::thread::hardware_concurrency())
    {
        m_threads.reserve(NUM_THREADS);
        for ([[maybe_unused]]
             auto threadNum : std::views::iota(0) | std::views::take(NUM_THREADS))
        {
            m_threads.emplace_back([this](const std::stop_token& stop) { runner(stop); });
        }
    }

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
                 auto threadNum : std::views::iota(m_threads.size()) | std::views::take(NUM_THREADS - m_threads.size()))
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
    auto enqueue(CallableType&& callable, ArgTypes&&... args) -> std::future<PromiseType>
    {
        using ReturnType = std::invoke_result_t<CallableType, ArgTypes...>;
        auto task        = std::make_unique<InternalDetail::Task<ReturnType, PromiseType, ArgTypes...>>(
          std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...
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
            std::unique_ptr<InternalDetail::ITask<PromiseType>>
              task;    // fetched task gets destructed after the while loop
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cvTaskReady.wait(lock, [this, &stop] { return !m_taskQueue.empty() || stop.stop_requested(); });
                if (stop.stop_requested())
                {
                    break;
                }
                task = std::move(m_taskQueue.front());
                m_taskQueue.pop();
            }
            task->run();
        }
    }

    auto enqueue(std::unique_ptr<InternalDetail::ITask<PromiseType>>&& task) -> std::future<PromiseType>
    {
        auto future = task->getFuture();

        {
            const std::lock_guard<std::mutex> LOCK(m_queueMutex);
            m_taskQueue.push(std::move(task));
        }
        m_cvTaskReady.notify_one();
        return future;
    }

    std::queue<std::unique_ptr<InternalDetail::ITask<PromiseType>>> m_taskQueue;
    std::vector<std::jthread>                                       m_threads;
    std::mutex                                                      m_queueMutex;
    std::condition_variable                                         m_cvTaskReady;
};
}    // namespace ThreadPool
