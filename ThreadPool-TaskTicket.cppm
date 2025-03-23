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

    [[nodiscard]]
    auto checkReady() const noexcept -> bool
    {
        using std::chrono_literals::operator""ms;
        return m_future.wait_for(0ms) == std::future_status::ready;
    }

    [[nodiscard]]
    auto isValid() const noexcept -> bool
    {
        return m_future.valid();
    }

    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> PromiseType
        requires (!std::is_void_v<PromiseType>)
    {
        return m_future.get();
    }

    void get()
        requires std::is_void_v<PromiseType>
    {
        m_future.get();
    }

    template <typename T>
        requires std::same_as<PromiseType, std::any> && (!std::same_as<T, std::any>) && (!std::is_void_v<T>)
    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> T
    {
        // PromiseType is any, and T is not any or void.
        return std::any_cast<T>(m_future.get());
    }

    template <typename T>
        requires std::same_as<PromiseType, T> && (!std::is_void_v<T>)
    [[nodiscard("Use the value or call get<void>() to discard the value explicitly.")]]
    auto get() -> T
    {
        // T is the same as PromiseType, neither is void
        return m_future.get();
    }

    template <typename T>
        requires std::is_void_v<T>
    void get()
    {
        m_future.get();
    }
};

}    // namespace ThreadPool
