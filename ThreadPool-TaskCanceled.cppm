module;

#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

export module ThreadPool:TaskCanceled;

namespace ThreadPool
{
export class TaskCanceled : public std::runtime_error
{
  public:
    TaskCanceled() = delete;
    TaskCanceled(const TaskCanceled&) = default;
    TaskCanceled(TaskCanceled&&)      = default;
    auto operator= (const TaskCanceled&) -> TaskCanceled& = default;
    auto operator= (TaskCanceled&&) -> TaskCanceled& = default;
    ~TaskCanceled() = default;

    explicit TaskCanceled(std::convertible_to<std::string> auto&& msg) noexcept
        : std::runtime_error(std::string(std::forward<decltype(msg)>(msg)))
    {
        // empty
    }
};
}    // namespace ThreadPool
