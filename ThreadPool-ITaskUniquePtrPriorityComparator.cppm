module;

#include <memory>

export module ThreadPool:ITaskUniquePtrPriorityComparator;

import :ITask;

namespace ThreadPool::InternalDetail
{
// no export, this is internal

/// @struct ITaskUniquePtrPriorityComparator
/// @brief Comparison function for tasks in the queue.
///
/// This comparison function is used to order tasks in the queue.
/// It compares the priorities of the tasks and orders them in descending
/// order of priority. For equal priorities, it orders the tasks in
/// ascending order of their TaskID.
///
/// @tparam PromiseType type of the result of the task
template <typename PromiseType>
struct ITaskUniquePtrPriorityComparator
{
    /// @brief Comparison function for tasks in the queue.
    ///
    /// This comparison function is used to order tasks in the queue.
    /// It compares the priorities of the tasks and orders them in descending
    /// order of priority. For equal priorities, it orders the tasks in
    /// ascending order of their TaskID.
    ///
    /// @param lhs left operand (task)
    /// @param rhs right operand (task)
    /// @returns true if rhs has higher priority, false otherwise
    [[nodiscard]]
    auto operator() (
      const std::unique_ptr<ITask<PromiseType>>& lhs, const std::unique_ptr<ITask<PromiseType>>& rhs
    ) const noexcept -> bool
    {
        // true means rhs has higher priority
        if (!lhs)
        {
            return true;    // lhs is invalid, rhs has priority
        }
        if (!rhs)
        {
            return false;    // rhs is invalid, lhs has priority
        }

        if (lhs->getPriority() == rhs->getPriority())
        {
            // if same priority, earlier tasks have higher priority
            return lhs->getTaskID() > rhs->getTaskID();
        }

        // higher numbers have higher priority
        return lhs->getPriority() < rhs->getPriority();
    }
};
}    // namespace ThreadPool::InternalDetail
