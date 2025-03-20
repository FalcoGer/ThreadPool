#include "ThreadPool.hpp"

#include <any>
#include <print>

auto main() -> int
{
    ThreadPool tp {};

    auto l = []() { std::println("Hello world"); };

    auto future = tp.enqueue(l);

    auto l2 = [a = 42](const float b, const double c) -> int { return static_cast<int>(a + (b * c)); };
    auto future2 = tp.enqueue(l2, 3.14F, 6.9);

    auto l3 = [](int& ref) { ref++; };
    int x = 2;
    auto future3 = tp.enqueue(l3, std::ref(x));

    future.get();
    std::println("{}", std::any_cast<int>(future2.get()));
    future3.get(); // block until done
    std::println("expecting x == 3, got x == {}", x);

    return 0;
}
