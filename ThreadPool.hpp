#pragma once

#include "ITask.hpp"
#include "Task.hpp"
#include <condition_variable>
#include <future>
#include <memory>
#include <queue>
#include <ranges>
#include <stop_token>
#include <thread>
#include <vector>

template <typename PromiseType = std::any>
class ThreadPool
{
  public:
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency())
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
                      std::unique_ptr<ITask<PromiseType>> task;    // fetched task gets destructed after the while loop
                      {
                          std::unique_lock<std::mutex> lock(m_queueMutex);
                          m_cvTaskReady
                            .wait(lock, [this, &stop] { return !m_taskQueue.empty() || stop.stop_requested(); });
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

    ~ThreadPool()
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

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueue(CallableType&& callable, ArgTypes&&... args) -> std::future<PromiseType>
    {
        using ReturnType = std::invoke_result_t<CallableType, ArgTypes...>;
        auto task        = std::make_unique<
                 Task<ReturnType, PromiseType, ArgTypes...>>(std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...);
        return enqueue(std::move(task));
    }

    auto enqueue(std::unique_ptr<ITask<PromiseType>>&& task) -> std::future<PromiseType>
    {
        auto future = task->getFuture();

        {
            const std::lock_guard<std::mutex> LOCK(m_queueMutex);
            m_taskQueue.push(std::move(task));
        }
        m_cvTaskReady.notify_one();
        return future;
    }

    ThreadPool(const ThreadPool&)                     = delete;
    auto operator= (const ThreadPool&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&)                          = delete;
    auto operator= (ThreadPool&&) -> ThreadPool&      = delete;

  private:
    std::queue<std::unique_ptr<ITask<PromiseType>>> m_taskQueue;
    std::vector<std::jthread>                       m_threads;
    std::mutex                                      m_queueMutex;
    std::condition_variable                         m_cvTaskReady;
};
