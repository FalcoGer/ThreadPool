#include <any>
#include <chrono>
#include <numbers>
#include <print>
#include <stdexcept>
#include <thread>
#include <set>

import ThreadPool;

auto main() -> int
{
    using ThreadPool::TaskID;
    using ThreadPool::ThreadPool;
    // NOLINTBEGIN (readability-magic-numbers) // example code
    {
        ThreadPool tp {};    // std::any by default

        // return type: void
        auto       l = []()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::println("Hello world");
        };
        auto ticket1 = tp.enqueue(l);
        // we let ticket go out of scope without calling get(). The thread will block upon destruction of tp until the
        // task is done.

        // return type: int
        auto l2      = [a = 42](const float b, const double c) -> int { return static_cast<int>(a + (b * c)); };
        auto ticket2 = tp.enqueue(l2, 3.14F, 6.9);

        // return type void, we take a reference though.
        auto l3      = [](int& ref) { ref++; };
        int  x       = 2;
        auto ticket3 = tp.enqueue(l3, x);
        // warning: potential UB: data race if x is accessed before ticket3 is done.

        // we know ticket2's lambda returned an int, so we can any_cast to int.
        std::println("{}", std::any_cast<int>(ticket2.get()));
        ticket3.get();    // block until done, waiting for the lambda to modify x
        // access to x is now safe again.
        std::println("expecting x == 3, got x == {}", x);
    }
    {
        ThreadPool<void> tp {};
        // PromiseType is void, the return value is discarded and can not be retrieved.
        auto             ticket = tp.enqueue([]() { return 42; });
    }
    {
        ThreadPool<int> tp {};
        // PromiseType is int, the return value is cast to int.
        auto            ticket  = tp.enqueue([]() { return 42; });
        // double gets cast to int before being assigned to the promise.
        auto            ticket2 = tp.enqueue([]() { return std::numbers::pi; });

        // error, incompatible types. PromiseType is int, return type is void.
        // If PromiseType is not std::any or void, then callable MUST return a value.
        // auto ticket3 = tp.enqueue([]() { return; });

        // error, incompatible types. PromiseType is int, return type is const char*
        // can not convert const char* to int.
        // auto ticket4 = tp.enqueue([]() { return "hello"; });

        std::println("{}", ticket.get());
        std::println("{}", ticket2.get());
    }
    {
        ThreadPool<int>  tp {};
        std::stop_source sc;
        auto             ticket = tp.enqueue(
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
        auto ticket2 = tp.enqueue(
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
        std::println("{} {}", ticket.get(), ticket2.get());
    }
    {
        ThreadPool<int> tp {};
        auto            ticket = tp.enqueue([]() -> int { throw std::runtime_error("Exception inside task"); });
        try
        {
            std::println("No exception yet.");
            std::println("ticket value = {}", ticket.get());
            std::println("Task completed.");
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }
    }
    {
        ThreadPool<int> tp {};
        int             x {8};
        int             y {};
        int             z {};
        auto            ticket = tp.enqueue(
          [](int& x) -> int
          {
              std::this_thread::sleep_for(std::chrono::seconds(3));
              std::println("task1 done");
              return x *= 2;
          },
          x
        );
        std::set<TaskID> deps;
        deps.insert(ticket.getTaskID());
        auto ticket2 = tp.enqueueWithDependencies(
          deps,
          [](int x, int& y) -> int
          {
              std::chrono::seconds(3);
              std::println("task2 done");
              return y = x + 1;
          },
          x,
          y
        );
        auto ticket3 = tp.enqueueWithDependencies(
          deps,
          [](int x, int& z) -> int
          {
              std::chrono::seconds(2);
              std::println("task3 done");
              return z = x / 4;
          },
          x,
          z
        );
        deps.clear();
        deps.insert(ticket2.getTaskID());
        deps.insert(ticket3.getTaskID());

        auto ticket4 = tp.enqueueWithDependencies(
          deps,
          [](int x, int y, int z) -> int
          {
              std::chrono::seconds(1);
              std::println("task4 done");
              return x + y + z;
          },
          x,
          y,
          z
        );

        std::println("ticket value = {}", ticket.get());
        std::println("ticket2 value = {}", ticket2.get());
        std::println("ticket3 value = {}", ticket3.get());
        std::println("ticket4 value = {}", ticket4.get());
        std::println("x = {}, y = {}, z = {}", x, y, z);
    }
    {
        ThreadPool<void> tp {};
        auto             ticket = tp.enqueue(
          []()
          {
              std::this_thread::sleep_for(std::chrono::seconds(3));
              throw std::runtime_error("Task Failed");
          }
        );
        std::set<TaskID> deps{};
        deps.insert(ticket.getTaskID());
        auto ticket2 = tp.enqueueWithDependencies(
          deps,
          []()
          {
              std::this_thread::sleep_for(std::chrono::seconds(3));
              std::println("Task2 depending on 1 done");
          }
        );
        std::set<TaskID> deps2 {};
        deps2.insert(ticket2.getTaskID());
        auto ticket3 = tp.enqueueWithDependencies(
          deps2,
          []()
          {
              std::this_thread::sleep_for(std::chrono::seconds(3));
              std::println("Task3 depending on 2 done");
          }
        );
        std::this_thread::sleep_for(std::chrono::seconds(4)); // let task0 fail before adding task depending on it
        auto ticket4 = tp.enqueueWithDependencies(
            deps,
            []()
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::println("Task4 depending on 1 done");
            }
          );
        try
        {
            ticket.get();
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }

        try
        {
            ticket2.get();
        }
        catch (const ::ThreadPool::TaskCanceled& ex)
        {
            std::println("TaskCanceled: {}", ex.what());
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }

        try
        {
            ticket3.get();
        }
        catch (const ::ThreadPool::TaskCanceled& ex)
        {
            std::println("TaskCanceled: {}", ex.what());
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }

        try
        {
            ticket4.get();
        }
        catch (const ::ThreadPool::TaskCanceled& ex)
        {
            std::println("TaskCanceled: {}", ex.what());
        }
        catch (const std::runtime_error& ex)
        {
            std::println("Exception caught: {}", ex.what());
        }
    }
    // NOLINTEND (readability-magic-numbers)
    return 0;
}
