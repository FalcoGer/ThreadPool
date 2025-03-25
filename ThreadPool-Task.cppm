module;

#include <any>
#include <concepts>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

export module ThreadPool:Task;

import :ITask;
import :TaskID;
import :TaskPriority;

namespace ThreadPool::InternalDetail
{
// no export, this is internal
template <typename ReturnType, typename PromiseType, typename... ArgTypes>
    requires (
      std::convertible_to<ReturnType, PromiseType> || std::is_void_v<PromiseType> || std::same_as<PromiseType, std::any>
    )
class Task : public ITask<PromiseType>
{
  private:
    std::function<ReturnType(ArgTypes...)> m_function;
    std::tuple<ArgTypes...>                m_args;

  public:
    template <typename CallableType>
        requires std::regular_invocable<CallableType, ArgTypes...>
                   && std::same_as<std::invoke_result_t<CallableType, ArgTypes...>, ReturnType>
    Task(
      const std::uint32_t TASK_ID,
      const TaskPriority  PRIORITY,
      std::set<TaskID>&&  dependencies,
      CallableType&&      callable,
      ArgTypes&&... args
    )
            : ITask<PromiseType> {TASK_ID, PRIORITY, std::move(dependencies)},
              m_function {std::forward<CallableType>(callable)},
              m_args {std::forward<ArgTypes>(args)...}
    {
        // empty
    }

    Task()                                          = delete;
    ~Task() override                                = default;
    Task(const Task& other)                         = delete;
    auto operator= (const Task& other) -> Task&     = delete;
    Task(Task&& other) noexcept                     = default;
    auto operator= (Task&& other) noexcept -> Task& = default;

    void run() override
    {
        if (this->getState() != ETaskState::WAITING)
        {
            // prevent restarting, we moved the arguments.
            throw std::runtime_error("Task already started");
        }
        this->setStarted();
        try
        {
            if constexpr (std::is_void_v<ReturnType>)
            {
                // No return from task
                std::apply(m_function, std::move(m_args));

                // we need to fulfill the promise anyway.
                // If promise type is any, construct empty any. Otherwise promise type is void, and we just set completed.
                if constexpr (std::same_as<PromiseType, std::any>)
                {
                    this->getPromise().set_value(std::any {});
                }
                else if constexpr (std::is_void_v<PromiseType>)
                {
                    this->getPromise().set_value();
                }
                else
                {
                    static_assert(false, "Promise type should be void, std::any, or ReturnType");
                }
            }
            else
            {
                // ReturnType of task is not void
                if constexpr (std::is_void_v<PromiseType>)
                {
                    // we have a return, but promise type is void, so we just set completed
                    this->getPromise().set_value();
                }
                else
                {
                    // set the value to the return
                    this->getPromise().set_value(static_cast<PromiseType>(std::apply(m_function, std::move(m_args))));
                }
            }
            this->setFinished();
        }
        catch (...)
        {
            this->setFailed(std::current_exception());
        }
    }
};
}    // namespace ThreadPool::InternalDetail
