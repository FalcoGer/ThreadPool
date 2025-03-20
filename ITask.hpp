#pragma once

#include <future>

namespace ThreadPool::InternalDetail
{
template <typename PromiseType>
class ITask
{
  private:
    bool                      m_isStarted = false;
    std::promise<PromiseType> m_promise;

  protected:
    void setStarted() noexcept { m_isStarted = true; }
    [[nodiscard]]
    auto getPromise() noexcept -> std::promise<PromiseType>&
    {
        return m_promise;
    }

  public:
    virtual ~ITask()   = default;

    virtual void run() = 0;

    [[nodiscard]]
    auto getResult() -> PromiseType
    {
        return m_promise.get_future().get();
    }

    [[nodiscard]]
    auto getIsStarted() const noexcept -> bool
    {
        return m_isStarted;
    }

    [[nodiscard]]
    auto getFuture() noexcept -> std::future<PromiseType>
    {
        return m_promise.get_future();
    }
};
}    // namespace ThreadPool::InternalDetail
