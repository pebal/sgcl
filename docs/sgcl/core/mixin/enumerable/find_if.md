[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](README.md)

# sgcl::mixin::enumerable\<Derived\>::find_if

```cpp
template<class Pred> constexpr auto find_if(Pred pred) noexcept(/* see below */);          // (1)
template<class Pred> constexpr auto find_if(Pred pred) const noexcept(/* see below */);    // (2)
```

Finds the first element `pred` accepts, calling it on the elements from the first on, and returns a pointer to
it rather than an iterator: null when there is none, so the result is tested and used in one `if`. On a range
whose iterator gives values rather than elements (`range(n)`, the [runes](../../runes/README.md) of a text) there is no
element to point to, and the value found is returned in an `optional`, empty when there is none: tested and read
in one `if` the same way.

1. A pointer through which the element may be written.
2. A pointer to a const element, on a const range.

- (1–2) Take part only when `pred` is a predicate callable with a const element; it asks nothing of the element,
  which needs no `==`.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the predicate, called with an element and returning what converts to `bool` |

## Return value

- (1) A pointer to the first element `pred` accepts (`T*` for elements of type `T`), null when it accepts none.
- (2) The same as a pointer to const (`const T*`).
- (1–2) On a range whose iterator gives values, an `optional<T>` with the value found, empty when `pred` accepts
  none (`optional<int>` for `range(n)`, `optional<char32_t>` for the runes of a text).

## Complexity

Linear in the size of the range: at most one call of `pred` per element.

## Exceptions

- (1) None when the copy of `pred` and its call on an element are noexcept; otherwise what they throw.
- (2) None when the copy of `pred` and its call on a const element are noexcept; otherwise what they throw.
- (1–2) On a range whose iterator gives values, also what the copy of the value into the `optional` throws.

## Notes

The pointer is valid as long as a reference to the element would be: a `push_back` that reallocates a `vector`
invalidates it, as it does an iterator. The `optional` of a range of values holds a copy, valid on its own.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    if (int* big = v.find_if([](int x) { return x > 8; })) {
        *big = 8;
    }
    println("{}", v);

    const vector<string> names = {"Ada", "Grace"};
    const string* found = names.find_if([](const string& s) { return s.size() > 3; });
    println("{}", *found);
    println("{}", names.find_if([](const string& s) { return s.empty(); }) == nullptr);

    if (auto square = range(1, 10).find_if([](int x) { return x * x > 20; })) {  // an optional<int>
        println("{}", *square);
    }
}
```

Output:

```text
[5, 3, 8, 3]
Grace
true
5
```

## See also

- [find_index](find_index.md): the position of the first element a predicate accepts
- [exists](exists.md): checks whether a predicate accepts some element
- [sgcl::mixin::enumerable\<Derived\>](README.md)
