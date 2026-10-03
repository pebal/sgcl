[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::operator std::chrono::nanoseconds

```cpp
constexpr operator std::chrono::nanoseconds() const noexcept;
```

Converts the duration into the standard's nanoseconds, implicitly: a function whose parameter is exactly
`std::chrono::nanoseconds` takes a duration as it is. A function template of the standard that deduces its
duration from the argument (`duration_cast`, `floor`, `round`, `this_thread::sleep_for`, `hh_mm_ss`,
`std::format`) does not look through a class, and takes `std::chrono::nanoseconds(d)`.

## Parameters

None.

## Return value

`std::chrono::nanoseconds` of the same count: exact, since both are 64 bits of nanoseconds.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;

long long count_of(std::chrono::nanoseconds n) {
    return n.count();
}

int main() {
    duration d = 90 * second;
    println("{}", count_of(d));  // converted implicitly

    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::nanoseconds(d));
    println("{}", minutes.count());
}
```

Output:

```text
90000000000
1
```

## See also

- [(constructor)](duration.md): the conversion from a `std::chrono` duration
- [nanoseconds](nanoseconds.md): the count as an `int64_t`
- [sgcl::duration](README.md)
