[sgcl](../../README.md) › [core](../README.md) › [generator](../generator.md)

# sgcl::generator\<T\>::generator

```cpp
generator() noexcept = default;
```

An empty generator: `next()` returns `false`, `begin() == end()`, `value()` may not be called. A generator with a
coroutine comes from calling a coroutine function that returns one, which makes the frame and hands it to the
generator without running the body.

The class declares no other constructor: its move constructor and move assignment are the implicit ones, which hand
the frame over and leave the source empty (the assignment destroys the coroutine it held first), both `noexcept`; a
generator is not copyable.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

generator<int> squares(int n) {
    for (int i : range(1, n + 1)) {
        co_yield i * i;
    }
}

int main() {
    generator<int> g;  // empty: no values
    println("{}", g.next());

    g = squares(4);  // the coroutine, not started yet
    generator<int> h = std::move(g);  // g empty again
    println("{} {}", g.next(), h.next());
    println("{}", std::is_copy_constructible_v<generator<int>>);
}
```

Output:

```text
false
false true
false
```

## See also

- [next](next.md): runs the coroutine to its next value
- [destroy](destroy.md): destroys the coroutine and leaves the generator empty
- [sgcl::generator\<T\>](../generator.md)
