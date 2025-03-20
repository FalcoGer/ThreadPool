## ThreadPool

A c++ ThreadPool implementation that allows tasks to return any data type they want, including none.

### Usage
See main.cpp for examples.

Calling `ThreadPool::enqueue(Callable, Arguments...)` returns an std::future<std::any>.
Call `.get()` on the future to lock until the thread is done, and use `std::any_cast<T>` to cast back to your expected type.
Exceptions are forwarded to the `std::future` as well.
