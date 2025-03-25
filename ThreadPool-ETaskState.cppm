module;

#include <cstdint>

export module ThreadPool:ETaskState;

namespace ThreadPool
{
export enum class ETaskState : std::uint8_t { PENDING = 0, RUNNING, FINISHED, FAILED, CANCELED };
}    // namespace ThreadPool
