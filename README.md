## ThreadPool

A c++ ThreadPool implementation that allows tasks to return any data type they want, including none.

### Usage
See main.cpp for examples.

1. Create a `ThreadPool`. You can pass a template argument for `PromiseType`. If none is provided, `std::any` is used by default. If the type is `void` all return values for the callables are discarded. If the type is not `std::any` or `void`, then all returns are cast to `PromiseType`, so make sure that that is possible. If the type is not `std::any` or `void` then the callable **must** return a value.
2. Call `ThreadPool::enqueue(Callable, Arguments...)`. This returns an `std::future<PromiseType>`.
3. The callable is put into the task queue. Call `.get()` on the future to block until the task is done. If `PromiseType` is `std::any`, use `std::any_cast<T>` to cast back to your expected type. If `PromiseType` is void, `.get()` will only block and not return a value. Otherwise the value will be returned. Exceptions are forwarded to the `std::future` as well and will be rethrown when `.get()` is called.

If you need cooperative cancelation, this is outside the scope of ThreadPool. Pass an `std::stop_token` from an `std::stop_source` into your callable and manage it yourself, or use something else.

Calling `resize(const std::size_t NUM_THREADS)` on the ThreadPool allows to add or remove threads. Removing threads will block until all removed threads executed their current task.

Calling `shutdownAndWait()` on a thread pool will stop all threads and block until their current tasks are done. A thread pool can be started up again with `resize`.

#### Simple Example

```cpp
// promise type is std::any by default, uses hardware concurrency for thread pool size by default.
// to use the same thread pool for different return value types, use std::any for the PromiseType
ThreadPool tp{};

// return type is bool, obviously you'd want some more heavy calculations in your threads.
auto even = [](const int value) -> bool { return value % 2 == 0; };
// return type is int
auto increment = [](const int value) -> int { return value + 1; };

// future type is std::future<std::any>
auto future = tp.enqueue(even, 42); // queues task for execution
auto future2 = tp.enqueue(increment, 42); // queues task for execution

// blocks execution until futures are ready
bool result = std::any_cast<bool>(future.get());
int result2 = std::any_cast<int>(future2.get());

// use the results
std::println("Value was {}", result ? "even" : "odd");
std::println("Increment is {}", result2);
```

### Types

| `PromiseType` | `ReturnType`  | Future Holds                            |
|---------------|---------------|-----------------------------------------|
| `std::any`    | `void`        | Empty `std::any{}`                      |
| `std::any`    | Not `void`    | `std::any` with value of return         |
| `void`        | `void`        | Nothing                                 |
| `void`        | Not `void`    | Nothing, return value discarded         |
| Other type    | `void`        | **Compiler error**                      |
| Other type    | Not `void`    | Return value cast to `PromiseType`      |

### Module

A module version is provided. Reference the CMakeLists.txt file and main.cpp on how to use it.

### Header

A header only version is also provided. Instead of `import ThreadPool`, simply `#include "ThreadPool.hpp"`.
