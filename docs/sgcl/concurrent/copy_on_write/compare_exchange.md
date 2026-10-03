[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](../copy_on_write.md)

# sgcl::concurrent::copy_on_write\<T\>::compare_exchange

```cpp
/*(1)*/ bool compare_exchange(snapshot& expected, const T& desired)
            noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ bool compare_exchange(snapshot& expected, T&& desired)
            noexcept(std::is_nothrow_move_constructible_v<T>);
```

Replaces the value with `desired` if the current value is still the one `expected` is a snapshot of; otherwise
leaves it and sets `expected` to a snapshot of the current value. The values are compared by identity, the
object `expected` holds, not by their contents.

1. A copy of `desired`.
2. `desired`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `expected` | a snapshot of the value the caller decided on; set to the current one when the exchange fails |
| `desired` | the new value |

## Return value

`true` when the value was replaced, `false` otherwise.

## Complexity

Constant: one allocation, the construction of one `T` and one compare-exchange.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

If an exception is thrown, the value and `expected` are as they were.

## Notes

[update](update.md) written out, for a change the caller decides from a snapshot and retries itself: lock-free,
the new value built before the exchange whether it succeeds or not.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::copy_on_write<int> version(1);

    auto seen = version.load();
    bool first = version.compare_exchange(seen, *seen + 1);
    println("{} {}", first, *version.load());

    auto stale = seen;  // still the snapshot of 1
    bool second = version.compare_exchange(stale, *stale + 1);
    println("{} {}", second, *stale);
}
```

Output:

```text
true 2
false 2
```

## See also

- [update](update.md): the copy, the change and the retry in one call
- [load](load.md): the snapshot to compare with
- [sgcl::concurrent::copy_on_write\<T\>](../copy_on_write.md)
