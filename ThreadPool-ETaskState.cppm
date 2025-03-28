module;

#include <cstdint>

export module ThreadPool:ETaskState;

namespace ThreadPool
{
/// @enum ETaskState
/// @brief Enum class to represent the state of a task.
export enum class ETaskState : std::uint8_t {
    /// @brief The task is waiting to be started.
    ///        This is the initial state of a task.
    PENDING = 0,

    /// @brief The task is currently running.
    RUNNING,

    /// @brief The task has finished successfully.
    FINISHED,

    /// @brief The task has finished with an error.
    FAILED,

    /// @brief The task was canceled.
    CANCELED
};
}    // namespace ThreadPool
