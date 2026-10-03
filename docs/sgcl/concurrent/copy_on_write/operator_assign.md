[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](../copy_on_write.md)

# sgcl::concurrent::copy_on_write\<T\>::operator=

```cpp
copy_on_write& operator=(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
copy_on_write& operator=(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
copy_on_write& operator=(const copy_on_write&) = delete;                                       // (3)
```

1. [store](store.md) of a copy of `value`.
2. [store](store.md) of `value`, moved.
3. A `copy_on_write` is not assignable from another: a shared value has one place.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the new value |

## Return value

`*this`.

## Complexity

Constant: one allocation, the construction of one `T` and one atomic store.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

If an exception is thrown, the value is as it was.

## Notes

Lock-free, as `store`; the later of two writers wins.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::copy_on_write<vector<int>> ports(vector<int>{80});
    ports = vector<int>{80, 443};
    println("{}", *ports.load());
}
```

Output:

```text
[80, 443]
```

## See also

- [store](store.md): the same, by name
- [sgcl::concurrent::copy_on_write\<T\>](../copy_on_write.md)
