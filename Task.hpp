#pragma once

#include "ITask.hpp"
#include <any>
#include <concepts>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename ReturnType, typename... ArgTypes>
class Task : public ITask
{
  private:
    std::function<ReturnType(ArgTypes...)> m_function;
    std::tuple<ArgTypes...>                m_args;

  public:
    template <typename CallableType>
        requires std::regular_invocable<CallableType, ArgTypes...>
                   && std::same_as<std::invoke_result_t<CallableType, ArgTypes...>, ReturnType>
    explicit Task(CallableType&& callable, ArgTypes&&... args)
            : m_function {std::forward<CallableType>(callable)}, m_args {std::forward<ArgTypes>(args)...}
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
        if (getIsStarted())
        {
            // prevent restarting, we moved the arguments.
            throw std::runtime_error("Task already started");
        }
        setStarted();
        try
        {
            getPromise().set_value(std::apply(m_function, std::move(m_args)));
        }
        catch (...)
        {
            getPromise().set_exception(std::current_exception());
        }
    }
};

// specialization is required because std::optional<void> is not allowed.
// empty tuple is allowed however.

// partial specialization for return type void
template <typename... ArgTypes>
class Task<void, ArgTypes...> : public ITask
{
  private:
    std::function<void(ArgTypes...)> m_function;
    std::tuple<ArgTypes...>          m_args;

  public:
    template <typename CallableType>
        requires std::regular_invocable<CallableType, ArgTypes...>
                   && std::same_as<std::invoke_result_t<CallableType, ArgTypes...>, void>
    explicit Task(CallableType&& callable, ArgTypes&&... args)
            : m_function {std::forward<CallableType>(callable)}, m_args {std::forward<ArgTypes>(args)...}
    {
        // empty
    }
    Task()                                      = delete;
    ~Task() override                            = default;
    Task(const Task& other)                     = delete;
    auto operator= (const Task& other) -> Task& = delete;
    Task(Task&& other)                          = default;
    auto operator= (Task&& other) -> Task&      = default;

    void run() override
    {
        if (getIsStarted())
        {
            throw std::runtime_error("Task already started");
        }
        setStarted();
        try
        {
            std::apply(m_function, std::move(m_args));
            getPromise().set_value(std::any {});
        }
        catch (...)
        {
            getPromise().set_exception(std::current_exception());
        }
    }
};
