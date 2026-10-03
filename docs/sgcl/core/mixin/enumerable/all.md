[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](README.md)

# sgcl::mixin::enumerable\<Derived\>::all

```cpp
template<class Pred> constexpr bool all(Pred pred) const noexcept(/* see below */);
```

Checks whether `pred` accepts every element, calling it on the elements from the first on and stopping at the
first it rejects. Takes part only when `pred` is a predicate callable with a const element; it asks nothing of the
element.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the predicate, called with a const element and returning what converts to `bool` |

## Return value

`true` when `pred` accepts every element, and on an empty range; `false` otherwise.

## Complexity

Linear in the size of the range: at most one call of `pred` per element.

## Exceptions

None when the copy of `pred` and its call on a const element are noexcept; otherwise what they throw.

## Notes

`all` is `std::ranges::all_of`, as a member.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{} {}", v.all([](int x) { return x > 3; }), v.all([](int x) { return x > 0; }));

    set<string> tags = {"red", "green"};
    println("{}", tags.all([](const string& t) { return !t.empty(); }));
    println("{}", vector<int>().all([](int) { return false; }));
}
```

Output:

```text
false true
true
true
```

## See also

- [exists](exists.md): checks whether a predicate accepts some element
- [count_of](count_of.md): the number of elements a predicate accepts
- [sgcl::mixin::enumerable\<Derived\>](README.md)
