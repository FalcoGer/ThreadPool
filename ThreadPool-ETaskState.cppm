module;

#include <cstdint>

export module ThreadPool:ETaskState;

namespace ThreadPool
{
export enum class ETaskState : std::uint8_t { WAITING = 0, STARTED, FINISHED, FAILED, CANCELED };
}    // namespace ThreadPool
