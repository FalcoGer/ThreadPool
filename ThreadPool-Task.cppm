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

/// @class Task
/// @brief Represents a task that can be executed in the thread pool.
///
/// This class represents a task that can be executed in the thread pool.
/// It is used internally by the thread pool to manage and execute tasks.
///
/// @tparam ReturnType type of the return value of the task
/// @tparam PromiseType type of the value returned through the future
/// @tparam ArgTypes types of the arguments passed to the task
///
/// @note This class is non-copyable
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
    /// @brief Constructor for Task.
    ///
    /// Constructs a Task with the given function, arguments, and dependencies.
    ///
    /// @param TASK_ID the ID of the task
    /// @param PRIORITY the priority of the task
    /// @param dependencies the dependencies of the task
    /// @param callable the function to be executed
    /// @param args the arguments to be passed to the function
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

    /// @brief Execute the task.
    ///
    /// This function is called by the thread pool to execute the task.
    /// It is not intended to be called directly by the user.
    ///
    /// If the task is already running or finished, this function
    /// throws a `std::runtime_error`.
    ///
    /// The task's state is changed to `ETaskState::RUNNING` before
    /// execution and to `ETaskState::FINISHED` or `ETaskState::FAILED`
    /// after execution, depending on whether an exception was thrown.
    ///
    /// If the task's return type is `void`, the promise is either set
    /// to either `std::any{}` or `void` depending on the promise type.
    /// If the promise type is not `void` or `std::any`, the return
    /// value of the task is cast to `PromiseType` and set to the
    /// promise.
    void run() override
    {
        if (this->getState() != ETaskState::PENDING)
        {
            // prevent restarting, we moved the arguments.
            throw std::runtime_error("Task already running or finished.");
        }
        this->setRunning();
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
