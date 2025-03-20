#pragma once

#include "ITask.hpp"
#include "Task.hpp"
#include <condition_variable>
#include <future>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
  public:
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename CallableType, typename... ArgTypes>
        requires std::regular_invocable<CallableType, ArgTypes...>
    [[nodiscard]]
    auto enqueue(CallableType&& callable, ArgTypes&&... args) -> std::future<std::any>
    {
        auto task = std::make_unique<Task<std::invoke_result_t<CallableType, ArgTypes...>, ArgTypes...>>(
          std::forward<CallableType>(callable), std::forward<ArgTypes>(args)...
        );
        return enqueue(std::move(task));
    }

    auto enqueue(std::unique_ptr<ITask>&& task) -> std::future<std::any>;

    ThreadPool(const ThreadPool&)                     = delete;
    auto operator= (const ThreadPool&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&)                          = delete;
    auto operator= (ThreadPool&&) -> ThreadPool&      = delete;

  private:
    std::queue<std::unique_ptr<ITask>> m_taskQueue;
    std::vector<std::jthread>          m_threads;
    std::mutex                         m_queueMutex;
    std::condition_variable            m_cvTaskReady;
};
