module;

#include <concepts>
#include <stdexcept>
#include <string>
#include <utility>

export module ThreadPool:TaskCanceled;

namespace ThreadPool
{
/// @class TaskCanceled
/// @brief Exception class for tasks that have been canceled.
///
/// This exception class is thrown by tasks that have been canceled.
/// The exception contains a message that describes why the task was canceled.
///
/// @note This class inherits from std::runtime_error
export class TaskCanceled : public std::runtime_error
{
  public:
    TaskCanceled()                                        = delete;
    TaskCanceled(const TaskCanceled&)                     = default;
    TaskCanceled(TaskCanceled&&)                          = default;
    auto operator= (const TaskCanceled&) -> TaskCanceled& = default;
    auto operator= (TaskCanceled&&) -> TaskCanceled&      = default;
    ~TaskCanceled()                                       = default;


    /// @brief Construct a TaskCanceled exception with a message.
    ///
    /// This constructor initializes the TaskCanceled exception with a message
    /// that describes the reason for the task cancellation. The message is passed
    /// to the base class std::runtime_error.
    ///
    /// @param msg A message describing the reason for task cancellation.
    explicit TaskCanceled(std::convertible_to<std::string> auto&& msg) noexcept
            : std::runtime_error(std::string(std::forward<decltype(msg)>(msg)))
    {
        // empty
    }
};
}    // namespace ThreadPool
