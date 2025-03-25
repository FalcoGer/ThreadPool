module;

#include <memory>

export module ThreadPool:ITaskUniquePtrPriorityComparator;

import :ITask;

namespace ThreadPool::InternalDetail
{
// no export, this is internal
template <typename PromiseType>
struct ITaskUniquePtrPriorityComparator
{
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
