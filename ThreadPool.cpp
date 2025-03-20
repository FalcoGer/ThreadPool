#include "ThreadPool.hpp"

#include <ranges>
#include <stop_token>

ThreadPool::ThreadPool(std::size_t numThreads)
{
    m_threads.reserve(numThreads);
    for ([[maybe_unused]]
         auto threadNum : std::views::iota(0) | std::views::take(numThreads))
    {
        m_threads.emplace_back(
          [this](const std::stop_token& stop)
          {
              while (!stop.stop_requested())
              {
                  std::unique_ptr<ITask> task; // fetched task gets destructed after the while loop
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
        );
    }
}

ThreadPool::~ThreadPool()
{
    {
        const std::lock_guard<std::mutex> LOCK(m_queueMutex);
        for (auto& thread : m_threads)
        {
            thread.request_stop();
        }
    }
    m_cvTaskReady.notify_all();
    // jthreads join automatically on destruction
}

auto ThreadPool::enqueue(std::unique_ptr<ITask>&& task) -> std::future<std::any>
{
    auto future = task->getFuture();

    {
        const std::lock_guard<std::mutex> LOCK(m_queueMutex);
        m_taskQueue.push(std::move(task));
    }
    m_cvTaskReady.notify_one();
    return future;
}
