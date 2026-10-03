[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::index_of

```cpp
constexpr size_t index_of(const auto& value) const noexcept(/* see below */);
```

Finds the position of the first element equal to `value`, comparing them with `==` from the first element on.
Takes part only when the elements are `req::equatable`; `value` may be of any type the elements compare with.
The position is the count of elements before it, whatever the iterator: on a `list` or a `sorted_set` as on a
`vector`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to look for |

## Return value

The position of the first element equal to `value`, `npos` when there is none.

## Complexity

Linear in the size of the range: at most one comparison per element.

## Exceptions

None when the `==` of an element with `value` is noexcept; otherwise what it throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.index_of(3));
    println("{}", v.index_of(7) == npos);

    sorted_set<int> s = {30, 10, 20};
    println("{}", s.index_of(30));
}
```

Output:

```text
1
true
2
```

## See also

- [last_index_of](last_index_of.md): the position of the last element equal to a value
- [find_index](find_index.md): the position of the first element a predicate accepts
- [contains](contains.md): checks whether an element is equal to a value
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
