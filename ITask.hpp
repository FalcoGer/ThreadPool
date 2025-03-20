#pragma once

#include <any>
#include <future>

class ITask
{
  private:
    bool                                    m_isStarted = false;
    std::promise<std::any>                  m_promise;

  protected:
    void setStarted() noexcept { m_isStarted = true; }
    [[nodiscard]]
    auto getPromise() noexcept -> std::promise<std::any>&
    {
        return m_promise;
    }

  public:
    virtual ~ITask()   = default;

    virtual void run() = 0;

    [[nodiscard]]
    auto getResult() -> std::any
    {
        return m_promise.get_future().get();
    }

    [[nodiscard]]
    auto getIsStarted() const noexcept -> bool
    {
        return m_isStarted;
    }

    [[nodiscard]]
    auto getFuture() noexcept -> std::future<std::any>
    {
        return m_promise.get_future();
    }
};
