module;

#include <any>
#include <atomic>
#include <format>
#include <functional>
#include <future>
#include <stdexcept>

export module ThreadPool:TaskTicket;

import :TaskID;
import :ETaskState;
import :ITask;

namespace ThreadPool
{

export template <typename PromiseType = std::any>
class ThreadPool;

export template <typename PromiseType = std::any>
class TaskTicket
{
  private:
    TaskID                                   m_taskId;
    std::shared_ptr<std::atomic<ETaskState>> m_taskState;
    std::future<PromiseType>                 m_future;

    friend ThreadPool<PromiseType>;
    // private constructor can only be called by ThreadPool
    TaskTicket(
      const TaskID TASK_ID, std::shared_ptr<std::atomic<ETaskState>> taskState, std::future<PromiseType>&& future
    )
            : m_taskId(TASK_ID), m_taskState(std::move(taskState)), m_future(std::move(future))
    {
        if (!m_taskState)
        {
            throw std::invalid_argument("taskState shared ptr was null");
        }
        if (!m_future.valid())
        {
            throw std::invalid_argument("future was not valid");
        }
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
    auto getState() const noexcept -> ETaskState
    {
        return m_taskState->load();
    }

    [[nodiscard]]
    auto isReady() const noexcept -> bool
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

template <typename PromiseType>
// NOLINTNEXTLINE(cert-dcl58-cpp) // specialization for std::hash is okay.
struct std::hash<ThreadPool::TaskTicket<PromiseType>>
{
    auto operator()(const ThreadPool::TaskTicket<PromiseType>& ticket) const -> std::size_t
    {
        return std::hash<ThreadPool::TaskID> {}(ticket.getTaskID());
    }
};

template <typename PromiseType>
// NOLINTNEXTLINE(cert-dcl58-cpp) // specialization for std::formatter is okay.
struct std::formatter<ThreadPool::TaskTicket<PromiseType>>
{
    auto format(const ThreadPool::TaskTicket<PromiseType>& ticket, std::format_context& ctx) const
    {
        std::string status = "Retrieved";
        if (ticket.isValid())
        {
            switch (ticket.getState())
            {
                case ThreadPool::ETaskState::WAITING:
                    status = "Pending"; break;
                case ThreadPool::ETaskState::STARTED:
                    status = "Running"; break;
                case ThreadPool::ETaskState::FINISHED:
                    status = "Finished"; break;
                case ThreadPool::ETaskState::FAILED:
                    status = "Failed"; break;
                case ThreadPool::ETaskState::CANCELED:
                    status = "Canceled"; break;
            }
        }
        return std::format_to(ctx.out(), "Task {} ({})", ticket.getTaskID(), status);
    }
};
