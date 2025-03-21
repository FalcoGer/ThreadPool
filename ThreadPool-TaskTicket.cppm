module;

#include <any>
#include <future>

export module ThreadPool:TaskTicket;

import :TaskID;

namespace ThreadPool
{

export template <typename PromiseType = std::any>
class ThreadPool;

export template <typename PromiseType = std::any>
class TaskTicket
{
  private:
    TaskID                   m_taskId;
    std::future<PromiseType> m_future;

    friend ThreadPool<PromiseType>;
    // private constructor can only be called by ThreadPool
    TaskTicket(const TaskID TASK_ID, std::future<PromiseType>&& future) : m_taskId(TASK_ID), m_future(std::move(future))
    {
        // empty
    }

  public:
    [[nodiscard]]
    auto getTaskID() const noexcept -> const TaskID&
    {
        return m_taskId;
    }

    [[nodiscard]]
    auto getFuture() noexcept -> std::future<PromiseType>&
    {
        return m_future;
    }

    auto get() -> PromiseType { return m_future.get(); }

    template <typename T>
        requires std::same_as<PromiseType, std::any>
    auto get() -> T
    {
        return std::any_cast<T>(m_future.get());
    }

    template <typename T>
        requires std::convertible_to<PromiseType, T> && (!std::same_as<PromiseType, std::any>) && (!std::is_void_v<T>)
    auto get() -> T
    {
        return static_cast<T>(m_future.get());
    }

    template <typename T>
        requires std::is_void_v<T>
    auto get() -> void
    {
        m_future.get();
    }
};

}    // namespace ThreadPool
