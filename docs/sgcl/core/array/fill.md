[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::fill

```cpp
constexpr void fill(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>);
```

Assigns `value` to every element. It hides the `fill` of [mixin::sequence](../mixin/sequence/README.md), which takes a
value of any type the elements are assignable from: the array's takes a `T`. For `array<T, 0>` it does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to assign |

## Return value

None.

## Complexity

Linear in `N`.

## Exceptions

What the copy assignment of `T` throws; none when it is noexcept. If it throws, the elements before the one whose
assignment threw hold `value`, the others their old values.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<int, 5> a = {1, 2, 3, 4, 5};
    a.fill(7);
    println("{}", a);

    array<tracked_ptr<int>, 3> roots = {};
    roots.fill(make_tracked<int>(42));  // three pointers to one object
    println("{} {}", *roots[2], roots[0] == roots[2]);

    roots.fill(nullptr);  // the object is the collector's now
    println("{}", roots[1] == nullptr);
}
```

Output:

```text
[7, 7, 7, 7, 7]
42 true
true
```

## See also

- [swap](swap.md): swaps the elements with another array's
- [sgcl::array\<T, N\>](README.md)
