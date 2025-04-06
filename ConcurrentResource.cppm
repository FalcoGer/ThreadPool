module;

#include <chrono>
#include <concepts>
#include <mutex>
#include <type_traits>
#include <utility>

export module ConcurrentResource;

template <typename Mutex>
concept IsMutex = std::same_as<Mutex, std::remove_cvref_t<Mutex>> && requires(Mutex& mtx) {
    mtx.lock();      // must have lock function
    mtx.unlock();    // must have unlock function
    { mtx.try_lock() } -> std::convertible_to<bool>;
};

template <typename Mutex>
concept IsSharedMutex = IsMutex<Mutex> && requires(Mutex& mtx) {
    mtx.lock_shared();
    mtx.unlock_shared();
    { mtx.try_lock_shared() } -> std::convertible_to<bool>;
};

template <typename Mutex, typename Duration, typename TimePoint = std::chrono::time_point<std::chrono::system_clock>>
concept IsTimedMutex =
  IsMutex<Mutex>
  && requires(Mutex& mtx, const std::remove_cvref_t<Duration> DUR, const std::remove_cvref_t<TimePoint> TIME_POINT) {
         { mtx.try_lock_for(DUR) } -> std::convertible_to<bool>;
         { mtx.try_lock_until(TIME_POINT) } -> std::convertible_to<bool>;
     };

template <typename Mutex, typename Duration, typename TimePoint = std::chrono::time_point<std::chrono::system_clock>>
concept IsSharedTimedMutex =
  IsSharedMutex<Mutex>
  && IsTimedMutex<Mutex, Duration, TimePoint>
  && requires(Mutex& mtx, const std::remove_cvref_t<Duration> DUR, const std::remove_cvref_t<TimePoint> TIME_POINT) {
         { mtx.try_lock_shared_for(DUR) } -> std::convertible_to<bool>;
         { mtx.try_lock_shared_until(TIME_POINT) } -> std::convertible_to<bool>;
     };

export template <typename Resource, IsMutex Mutex = std::mutex>
class ConcurrentResource
{
  public:
    template <typename... Args>
    explicit ConcurrentResource(Args&&... ctorArguments) : m_resource {std::forward<Args>(ctorArguments)...}
    {}

    ~ConcurrentResource() = default;

    [[nodiscard]]
    auto resource(this auto&& self) -> decltype(auto)
    {
        return self.m_resource;
    }

    auto operator*(this auto&& self) -> auto&& { return std::forward<decltype(self)>(self).m_resource; }
    auto operator->(this auto&& self) -> auto* { return &self.m_resource; }

    [[nodiscard]]
    auto mutex(this auto&& self) -> decltype(auto)
    {
        return self.m_mutex;
    }

    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    void lock() { m_mutex.lock(); }

    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    void unlock() { m_mutex.unlock(); }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock() -> bool
    {
        return m_mutex.try_lock();
    }

    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    void lock_shared()
        requires IsSharedMutex<Mutex>
    {
        m_mutex.lock_shared();
    }

    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    void unlock_shared()
        requires IsSharedMutex<Mutex>
    {
        m_mutex.unlock_shared();
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock_shared() -> bool
        requires IsSharedMutex<Mutex>
    {
        return m_mutex.try_lock_shared();
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock_for(const auto duration) -> bool
        requires IsTimedMutex<Mutex, decltype(duration)>
    {
        return m_mutex.try_lock_for(duration);
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock_until(const auto timepoint) -> bool
        requires IsTimedMutex<Mutex, std::chrono::seconds, decltype(timepoint)>
    {
        return m_mutex.try_lock_until(timepoint);
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock_shared_for(const auto duration) -> bool
        requires IsSharedTimedMutex<Mutex, decltype(duration)>
    {
        return m_mutex.try_lock_shared_for(duration);
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-identifier-naming) // Needs to be exactly this name
    auto try_lock_shared_until(const auto timepoint) -> bool
        requires IsSharedTimedMutex<Mutex, std::chrono::seconds, decltype(timepoint)>
    {
        return m_mutex.try_lock_shared_until(timepoint);
    }

  private:
    Resource m_resource;
    Mutex    m_mutex;
};
