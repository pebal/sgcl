[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::count_of

```cpp
template<class Pred> constexpr size_t count_of(Pred pred) const noexcept(/* see below */);
```

Counts the elements `pred` accepts, calling it on every element. Takes part only when `pred` is a predicate
callable with a const element; it asks nothing of the element.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the predicate, called with a const element and returning what converts to `bool` |

## Return value

The number of elements `pred` accepts.

## Complexity

Linear in the size of the range: exactly one call of `pred` per element.

## Exceptions

None when the copy of `pred` and its call on a const element are noexcept; otherwise what they throw.

## Notes

`count_of` is `std::ranges::count_if`, as a member. A container's own `count` (of a key, in a set or a map) is a
different question.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.count_of([](int x) { return x == 3; }));

    sorted_map<string, int> stock = {{"apple", 0}, {"bread", 4}, {"tea", 2}};
    println("{}", stock.count_of([](const auto& item) { return item.second > 0; }));
    println("{}", range(100).count_of([](int x) { return x % 7 == 0; }));
}
```

Output:

```text
2
2
15
```

## See also

- [exists](exists.md), [all](all.md): check whether a predicate accepts some element, every element
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
