[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::emplace

```cpp
template<class... A>
requires std::is_nothrow_constructible_v<T, A...>
T& emplace(A&&... a) noexcept;                                                  // (1)
template<class U, class... A>
requires std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>
T& emplace(std::initializer_list<U> il, A&&... a) noexcept;                     // (2)
```

Destroys the value or the error held and constructs a value in its place.

1. From `a...`.
2. From `il` and `a...`.

Takes part only when the construction cannot throw, as `std::expected` asks: the one held is destroyed first, and
nothing could be put back. A value whose construction may throw is assigned instead
([operator=](operator_assign.md)), which keeps the old one until the new one is made.

`expected<void, E>` has `void emplace() noexcept`: the error, if any, destroyed, and a success.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the value is constructed from |
| `il` | the initializer list the value is constructed from |

## Return value

A reference to the new value.

## Complexity

Constant, plus the destruction of the old value or error and the construction of the new value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    expected<Point, string> e = unexpected("not yet");
    Point& p = e.emplace(1, 2);  // the error destroyed
    p.x = 10;
    println("{} {} {}", e.has_value(), e->x, e->y);

    expected<void, string> done = unexpected("failed");
    done.emplace();
    println("{}", done.has_value());
}
```

Output:

```text
true 10 2
true
```

## See also

- [operator=](operator_assign.md): assigns a value made beforehand
- [sgcl::expected\<T, E\>](../expected.md)
