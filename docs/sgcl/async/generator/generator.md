[sgcl](../../README.md) › [async](../README.md) › [generator](../generator.md)

# sgcl::async::generator\<T\>::generator

```cpp
generator() noexcept = default;               // (1)
generator(generator&&) noexcept = default;    // (2)
```

Constructs a generator object.

1. An empty generator: [done](done.md) gives `true`, and `co_await g.next()` gives nothing at once.
2. Takes the coroutine of the other generator over; the other generator is empty after.

A generator with a coroutine comes from calling a coroutine function that returns one: the call allocates the frame,
constructs the promise and the generator, and suspends before the first statement of the body. The generator is not
copyable.

## Parameters

| Parameter | Description |
|---|---|
| `generator&&` | the generator whose coroutine is taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::generator<int> numbers(int count) {
    for (int i : range(count)) {
        co_yield i;
    }
}

async::task<> consume() {
    async::generator<int> empty;
    println("{} {}", empty.done(), (co_await empty.next()).has_value());

    auto g = numbers(3);
    async::generator<int> moved = std::move(g);
    println("{}", g.done());
    while (auto v = co_await moved.next()) {
        println("{}", *v);
    }
}

int main() {
    consume().wait();
}
```

Output:

```text
true false
true
0
1
2
```

## See also

- [operator=](operator_assign.md): destroys the coroutine held and takes another's over
- [next](next.md): runs the coroutine to its next value
- [sgcl::async::generator\<T\>](../generator.md)
