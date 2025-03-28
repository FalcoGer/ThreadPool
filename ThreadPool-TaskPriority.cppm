module;

#include <compare>
#include <cstdint>
#include <format>
#include <functional>

export module ThreadPool:TaskPriority;

namespace ThreadPool
{
/// @class TaskPriority
/// @brief Represents the priority of a task in the thread pool.
///
/// This class encapsulates the priority value for tasks within the thread pool.
/// A higher numerical value indicates a higher priority.
/// The priority is used to determine the order in which tasks are executed.
export class TaskPriority
{
  private:
    std::int16_t m_priority;

  public:
    /// @brief Default constructor for TaskPriority.
    ///
    /// Constructs a TaskPriority with a value of 0.
    /// @note This does not throw and is marked with @c noexcept
    TaskPriority() noexcept : TaskPriority {0}
    {
        // empty
    }

    /// @brief Constructor for TaskPriority.
    ///
    /// Constructs a TaskPriority with the given value.
    /// @param PRIORITY the priority value
    /// @note This does not throw and is marked with @c noexcept
    explicit TaskPriority(const std::int16_t PRIORITY) noexcept : m_priority(PRIORITY)
    {
        // empty
    }

    /// @brief Get the priority value.
    ///
    /// This method returns the priority value associated with the task.
    /// A higher numerical value indicates a higher priority.
    ///
    /// @returns The priority value as an integer.
    ///
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto get() const noexcept -> std::int16_t
    {
        return m_priority;
    }

    /// @brief Three-way comparison operator.
    ///
    /// This method compares the priority value of this object with the priority value of @p other.
    /// The comparison is done using the three-way comparison operator <=>, which returns a value of type @c
    /// std::strong_ordering.
    /// @param other the TaskPriority object to compare with
    /// @returns A value of type @c std::strong_ordering indicating the result of the comparison.
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto operator<=> (const TaskPriority& other) const noexcept -> std::strong_ordering
    {
        return m_priority <=> other.m_priority;
    }

    /// @brief Equality comparison operator.
    ///
    /// This method compares the priority value of this object with the priority value of @p other.
    /// Two TaskPriority objects are considered equal if their priority values are equal.
    /// @param[in] other the TaskPriority object to compare with
    /// @returns true if the priority values are equal, false otherwise
    /// @note This function does not throw and is marked with @c noexcept
    [[nodiscard]]
    auto operator== (const TaskPriority& other) const noexcept -> bool
    {
        return m_priority == other.m_priority;
    }
};
}    // namespace ThreadPool

template <>
struct std::formatter<ThreadPool::TaskPriority> : public std::formatter<std::int16_t>
{
    auto format(const ThreadPool::TaskPriority& taskPriority, format_context& ctx) const
    {
        return std::formatter<std::int16_t>::format(taskPriority.get(), ctx);
    }
};

template <>
struct std::hash<ThreadPool::TaskPriority>
{
    [[nodiscard]]
    auto operator() (const ThreadPool::TaskPriority& taskPriority) const noexcept -> std::size_t
    {
        return std::hash<std::int16_t> {}(taskPriority.get());
    }
};
