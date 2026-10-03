[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [sequence](../sequence.md)

# sgcl::mixin::sequence\<Derived\>::fill

```cpp
template<class V>
constexpr void fill(const V& value) noexcept(/* see below */)
    requires std::is_assignable_v<std::ranges::range_reference_t<Derived>, const V&>;
```

Assigns `value` to every element, from the first to the last. `value` may be of any type an element is assigned
from: a literal for a vector of strings is assigned as it is, with no element made of it first. Nothing is asked of
the element beyond that assignment; the number of elements does not change. Takes part only when an element is
assignable from `value`: `requires { v.fill(x); }` is `false` otherwise.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to assign to every element |

## Return value

None.

## Complexity

Linear in the size of the range: exactly one assignment per element.

## Exceptions

None when the assignment of `value` to an element is noexcept; otherwise what it throws.

If an exception is thrown, the elements before the one whose assignment threw hold `value`, and the ones after it
are as they were.

## Notes

`array` hides this one with a `fill` of its own, the one of `std::array`, which takes a `const T&`. A `slice<T>`
fills the elements of the container it views, and a `slice<const T>` has no `fill`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {3, 2, 1};
    slice<int> tail = v.as_slice(1);
    tail.fill(0);  // the slice writes the vector's elements
    println("{}", v);

    deque<string> names = {"Ada", "Grace"};
    names.fill("anonymous");
    println("{}", names);
}
```

Output:

```text
[3, 0, 0]
["anonymous", "anonymous"]
```

## See also

- [reverse](reverse.md): reverses the order of the elements
- [sgcl::mixin::sequence\<Derived\>](../sequence.md)
