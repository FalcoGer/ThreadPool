#include "ThreadPool.hpp"

#include <any>
#include <chrono>
#include <numbers>
#include <print>
#include <stdexcept>
#include <thread>

auto main() -> int
{
    using ThreadPool::ThreadPool;
    // NOLINTBEGIN (readability-magic-numbers) // example code
    {
        ThreadPool tp {}; // std::any by default

        // return type: void
        auto       l = []()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::println("Hello world");
        };
        auto       future  = tp.enqueue(l);
        // we let future go out of scope without calling get(). The thread will block upon destruction of tp until the
        // task is done.

        // return type: int
        auto       l2      = [a = 42](const float b, const double c) -> int { return static_cast<int>(a + (b * c)); };
        auto       future2 = tp.enqueue(l2, 3.14F, 6.9);

        // return type void, we take a reference though.
        auto       l3      = [](int& ref) { ref++; };
        int        x       = 2;
        auto       future3 = tp.enqueue(l3, x);

        // we know future2's lambda returned an int, so we can any_cast to int.
        std::println("{}", std::any_cast<int>(future2.get()));
        future3.get();    // block until done, waiting for the lambda to modify x
        std::println("expecting x == 3, got x == {}", x);
    }
    {
        ThreadPool<void> tp {};
        // PromiseType is void, the return value is discarded and can not be retrieved.
        auto future = tp.enqueue([]() { return 42; });
    }
    {
        ThreadPool<int> tp {};
        // PromiseType is int, the return value is cast to int.
        auto            future = tp.enqueue([]() { return 42; });
        // double gets cast to int before being assigned to the promise.
        auto            future2 = tp.enqueue([]() { return std::numbers::pi; });

        // error, incompatible types. PromiseType is int, return type is void.
        // If PromiseType is not std::any or void, then callable MUST return a value.
        // auto future3 = tp.enqueue([]() { return; });

        // error, incompatible types. PromiseType is int, return type is const char*
        // can not convert const char* to int.
        // auto future4 = tp.enqueue([]() { return "hello"; });

        std::println("{}", future.get());
        std::println("{}", future2.get());
    }
    {
        ThreadPool<int>  tp {};
        std::stop_source sc;
        auto             future = tp.enqueue(
          [](const std::stop_token& stop)
          {
              unsigned int i {};
              while (!stop.stop_requested())
              {
                  i++;
                  std::this_thread::sleep_for(std::chrono::milliseconds(100));
              }
              return i;
          },
          sc.get_token()
        );
        auto future2 = tp.enqueue(
          [](const std::stop_token& stop)
          {
              unsigned int i {};
              while (!stop.stop_requested())
              {
                  i++;
                  std::this_thread::sleep_for(std::chrono::milliseconds(25));
              }
              return i;
          },
          sc.get_token()
        );
        std::this_thread::sleep_for(std::chrono::seconds(1));
        sc.request_stop();
        std::println("{} {}", future.get(), future2.get());
    }
    {
        ThreadPool<int> tp {};
        auto            future = tp.enqueue([]() -> int { throw std::runtime_error("Exception inside task"); });
        try
        {
            std::println("No exception yet.");
            std::println("Future value = {}", future.get());
            std::println("Task completed.");
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }
    }
    // NOLINTEND (readability-magic-numbers)
    return 0;
}
