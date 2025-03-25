#include <any>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <numbers>
#include <print>
#include <stdexcept>
#include <thread>
#include <vector>

import ThreadPool;

namespace
{
struct WorkStruct
{
    auto operator() (int x, int y) const -> int
    {
        // NOLINTNEXTLINE(readability-magic-numbers) // example code
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return x * y;
    }
};

auto function(int x, int y) -> int
{
    return x - y;
}

auto function2(int x, int y) -> int
{
    return static_cast<int>(static_cast<unsigned int>(x) << static_cast<unsigned int>(y));
}
}    // namespace

auto main() -> int
{
    using ThreadPool::TaskID;
    using ThreadPool::TaskPriority;
    using ThreadPool::TaskTicket;
    using ThreadPool::ThreadPool;

    using namespace std::chrono_literals;
    // NOLINTBEGIN (readability-magic-numbers) // example code, actual values are meaningless and giving names is not helping readability.
    {
        ThreadPool tp {};    // std::any by default

        // return type: void
        auto       l = []()
        {
            std::this_thread::sleep_for(200ms);
            std::println("Hello world");
        };
        auto ticket1 = tp.enqueue(l);
        // we let ticket go out of scope without calling get(). The thread will block upon destruction of tp until the
        // task is done.

        // return type: int
        auto l2      = [a = 42](const float b, const double c) -> int
        { return static_cast<int>(a + (static_cast<double>(b) * c)); };
        auto ticket2 = tp.enqueue(l2, 3.14F, 6.9);
        auto ticket3 = tp.enqueue(l2, 1.23, 4.56);

        // return type void, we take a reference though.
        auto l4      = [](int& ref) { ref++; };
        int  x       = 2;
        std::println("Ticket 2: {}", ticket2);
        auto ticket4 = tp.enqueue(l4, x);
        // warning: potential UB: data race if x is accessed before ticket4 is done.

        // we know ticket2's lambda returned an int, so we can any_cast to int.
        std::println("{}", std::any_cast<int>(ticket2.get()));
        std::println("{}", ticket3.get<int>());
        ticket4.get<void>();    // block until done, waiting for the lambda to modify x
        // access to x is now safe again.
        std::println("expecting x == 3, got x == {}", x);
    }
    {
        ThreadPool<int>                    tp {};
        const auto                         lambda = [](int x, int y) -> int { return x + y; };
        const WorkStruct                   callableObject {};
        const std::function<int(int, int)> stdfunction {[](int x, int y) -> int { return x / y; }};
        int (*const fptr)(int, int) = function2;

        auto ticket1                = tp.enqueue(lambda, 2, 3);            // 2+3
        auto ticket2                = tp.enqueue(callableObject, 2, 3);    // 2*3
        auto ticket3                = tp.enqueue(stdfunction, 2, 3);       // 2/3
        auto ticket4                = tp.enqueue(function, 2, 3);          // 2-3
        auto ticket5                = tp.enqueue(fptr, 2, 3);              // 2<<3

        std::println("2+3  = {}", ticket1.get());
        std::println("2*3  = {}", ticket2.get());
        std::println("2/3  = {}", ticket3.get());
        std::println("2-3  = {}", ticket4.get());
        std::println("2<<3 = {}", ticket5.get());
    }
    {
        ThreadPool tp {2};
        auto       holdup = []() { std::this_thread::sleep_for(200ms); };
        // make all threads busy
        auto       _      = tp.enqueue(holdup);
        _                 = tp.enqueue(holdup);

        // add tasks with different priorities
        std::vector<TaskTicket<std::any>> tickets;
        auto                              work = [](const int x)
        {
            std::this_thread::sleep_for(200ms);
            std::println("task with priority {} is done.", x);
            return x;
        };
        tickets.push_back(tp.enqueueWithPriority(TaskPriority {0}, work, 0));
        tickets.push_back(tp.enqueueWithPriority(TaskPriority {2}, work, 2));
        tickets.push_back(tp.enqueueWithPriority(TaskPriority {-1}, work, -1));
        tickets.push_back(tp.enqueueWithPriority(TaskPriority {1}, work, 1));
        tickets.push_back(tp.enqueueWithPriority(TaskPriority {3}, work, 3));

        std::this_thread::sleep_for(50ms);

        for (auto&& ticket : tickets)
        {
            std::println("Task {}: {}", ticket.getTaskID(), ticket.get<int>());
        }
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
                  std::this_thread::sleep_for(100ms);
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
                  std::this_thread::sleep_for(25ms);
              }
              return i;
          },
          sc.get_token()
        );
        std::this_thread::sleep_for(1s);
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
          [](int& lx) -> int
          {
              std::this_thread::sleep_for(1s);
              std::println("task1 done");
              return lx = 2;
          },
          x
        );

        auto ticket2 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket.getTaskID()},
          [](int lx, int& ly) -> int
          {
              std::this_thread::sleep_for(300ms);
              std::println("task2 done");
              return ly = lx + 1;
          },
          x,
          y
        );
        auto ticket3 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket.getTaskID()},
          [](int lx, int& lz) -> int
          {
              std::this_thread::sleep_for(200ms);
              std::println("task3 done");
              return lz = lx + 2;
          },
          x,
          z
        );

        auto ticket4 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket2.getTaskID(), ticket3.getTaskID()},
          [](int lx, int ly, int lz) -> int
          {
              std::this_thread::sleep_for(100ms);
              std::println("task4 done");
              return lx + ly + lz;
          },
          x,
          y,
          z
        );

        std::println("ticket value = {}, expected 2", ticket.get());
        std::println("ticket2 value = {}, expected 3", ticket2.get());
        std::println("ticket3 value = {}, expected 4", ticket3.get());
        std::println("ticket4 value = {}, expected 2+3+4 = 9", ticket4.get());
        std::println("x = {} (expected 2), y = {} (expected 3), z = {} (expected 4)", x, y, z);
    }
    {
        ThreadPool<void> tp {};
        auto             ticket1 = tp.enqueue(
          []()
          {
              std::this_thread::sleep_for(3s);
              throw std::runtime_error("Task Failed");
          }
        );

        auto ticket2 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket1.getTaskID()},
          []()
          {
              std::this_thread::sleep_for(3s);
              std::println("Task2 depending on 1 done");
          }
        );

        auto ticket3 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket2.getTaskID()},
          []()
          {
              std::this_thread::sleep_for(3s);
              std::println("Task3 depending on 2 done");
          }
        );
        std::this_thread::sleep_for(500ms);    // let task0 fail before adding task depending on it
        auto ticket4 = tp.enqueueWithDependencies(
          std::initializer_list<TaskID> {ticket1.getTaskID()},
          []()
          {
              std::this_thread::sleep_for(1s);
              std::println("Task4 depending on 1 done");
          }
        );
        try
        {
            ticket1.get();
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
