[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](../copy_on_write.md)

# sgcl::concurrent::copy_on_write\<T\>::store

```cpp
/*(1)*/ void store(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ void store(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);
```

Replaces the value, whole: a new managed object holding the value, stored into the pointer.

1. A copy of `value`.
2. `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the new value |

## Return value

None.

## Complexity

Constant: one allocation, the construction of one `T` and one atomic store.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

If an exception is thrown, the value is as it was.

## Notes

Lock-free. A store does not look at the value it replaces: of two writers storing at once, the later store wins,
and a change another writer made with [update](update.md) meanwhile is overwritten. A change that depends on the
current value is an `update` or a [compare_exchange](compare_exchange.md). The old value stays alive while a
snapshot holds it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::copy_on_write<string> greeting(string("hello"));
    auto old = greeting.load();

    greeting.store("bonjour");
    println("{} {}", *old, *greeting.load());
}
```

Output:

```text
hello bonjour
```

## See also

- [operator=](operator_assign.md): the same, as an assignment
- [update](update.md): changes a copy of the current value
- [sgcl::concurrent::copy_on_write\<T\>](../copy_on_write.md)
