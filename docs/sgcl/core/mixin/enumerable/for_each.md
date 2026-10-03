[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::for_each

```cpp
template<class F> constexpr void for_each(F f) noexcept(/* see below */);          // (1)
template<class F> constexpr void for_each(F f) const noexcept(/* see below */);    // (2)
```

Calls `f` with every element, from the first to the last; what `f` returns is ignored.

1. Calls `f` with the elements as the iterator gives them, so `f` may write an element it takes by reference.
   Takes part only when `f` is callable with an element.
2. Calls `f` with const elements. Takes part only when `f` is callable with a const element.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function to call with every element |

## Return value

None.

## Complexity

Linear in the size of the range: exactly one call of `f` per element.

## Exceptions

- (1) None when the copy of `f` and its call on an element are noexcept; otherwise what they throw.
- (2) None when the copy of `f` and its call on a const element are noexcept; otherwise what they throw.

The elements `f` has written before it threw keep what it wrote.

## Notes

`f` is taken by value and called on that copy: what a stateful `f` gathers stays in the copy, so a sum or a
count is kept through a reference it captures, not in a member of its own.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list<int> l = {3, 1, 2};
    int sum = 0;
    l.for_each([&](int x) { sum += x; });
    println("{}", sum);

    vector v = {1, 2, 3};
    v.for_each([](int& x) { x *= 10; });
    println("{}", v);
}
```

Output:

```text
6
[10, 20, 30]
```

## See also

- [count_of](count_of.md): the number of elements a predicate accepts
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
