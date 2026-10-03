[sgcl](../../README.md) › [core](../README.md) › [any](../any.md)

# sgcl::any::operator=

```cpp
/*(1)*/ any& operator=(const any& o);
/*(2)*/ any& operator=(any&& o) noexcept;
/*(3)*/ template<class T, class VT = std::decay_t<T>>
        requires (!std::is_same_v<VT, any> && std::is_copy_constructible_v<VT>)
        any& operator=(T&& value) noexcept(std::is_nothrow_constructible_v<VT, T>);
```

Replaces the value held.

1. A copy of the value of `o`, if any; a value in a node is copied into a node of its own.
2. The value of `o`, taken over; `o` is empty after.
3. `std::forward<T>(value)` as a `VT`, placed as the [constructor](any.md) places it.

The new value is made first, in a temporary `any`, and swapped in; the old value is destroyed then, on the calling
thread. A value in a node goes with its node; the node is left to the collector.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `any` to copy or to take the value from |
| `value` | the value to hold |

## Return value

`*this`.

## Complexity

Constant: one managed allocation for a new value in a node (1, 3), none for the others.

## Exceptions

- (1) What the copy constructor of the value of `o` throws.
- (2) None.
- (3) What the constructor of `VT` throws; none when it is noexcept.

If an exception is thrown, the `any` is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    any a = 1;
    any b;
    b = a;  // a copy of the int
    a = string("text");  // the int destroyed, a string in a node of its own
    println("{} {}", any_cast<string&>(a), any_cast<int>(b));

    b = std::move(a);
    println("{} {}", a.has_value(), any_cast<string&>(b));
}
```

Output:

```text
text 1
false text
```

## See also

- [emplace](emplace.md): constructs the new value in place
- [reset](reset.md): destroys the value without a new one
- [sgcl::any](../any.md)
