## ThreadPool

A c++ ThreadPool implementation that allows tasks to return any data type they want, including none.

### Usage
See main.cpp for examples.

1. Create a `ThreadPool`. You can pass a template argument for `PromiseType`. If none is provided, `std::any` is used by default. If the type is `void` all return values for the callables are discarded. If the type is not `std::any` or `void`, then all returns are cast to `PromiseType`, so make sure that that is possible. If the type is not `std::any` or `void` then the callable **must** return a value.
2. Call `ThreadPool::enqueue(Callable, Arguments...)`. This returns an `std::future<PromiseType>`.
3. The callable is put into the task queue. Call `.get()` on the future to block until the thread is done. If `PromiseType` is `std::any`, use `std::any_cast<T>` to cast back to your expected type. If `PromiseType` is void, `.get()` will only block and not return a value. Otherwise the value will be returned. Exceptions are forwarded to the `std::future` as well and will be rethrown when `.get()` is called.

If you need cooperative cancelation, this is outside the scope of ThreadPool. Pass an `std::stop_token` from an `std::stop_source` into your callable and manage it yourself, or use something else.

### Types

| `PromiseType` | `ReturnType`  | Future Holds                            |
|---------------|---------------|-----------------------------------------|
| `std::any`    | `void`        | Empty `std::any{}`                      |
| `std::any`    | Not `void`    | `std::any` with value of return         |
| `void`        | `void`        | Nothing                                 |
| `void`        | Not `void`    | Nothing, return value discarded         |
| Other type    | `void`        | **Compiler error**                      |
| Other type    | Not `void`    | Return value cast to `PromiseType`      |
