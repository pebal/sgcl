[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::contains

```cpp
constexpr bool contains(const auto& value) const noexcept(/* see below */);
```

Checks whether some element of the range is equal to `value`, comparing them with `==` from the first element
on. Takes part only when the elements are `req::equatable`; `value` may be of any type the elements compare
with, a literal for a vector of strings.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to look for |

## Return value

`true` when an element is equal to `value`, `false` otherwise.

## Complexity

Linear in the size of the range: at most one comparison per element.

## Exceptions

None when the `==` of an element with `value` is noexcept; otherwise what it throws.

## Notes

`contains` is the question `std::ranges::find(r, value) != r.end()` asks, as a member of every container of the
library that iterates: a `vector`, a `deque`, a `list`, a `slice`, a `range`.

A container with a better answer hides this one with its own `contains`: a set or a map asks by the key, in
constant or logarithmic time. A slice of characters has a `contains` of its own, for text: it looks for a
character or a run of characters.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> names = {"Ada", "Grace", "Linus"};
    println("{} {}", names.contains("Grace"), names.contains("Bjarne"));

    deque<int> queue = {3, 1, 4};
    println("{}", queue.contains(4));

    println("{}", range(10).contains(7));
}
```

Output:

```text
true false
true
true
```

## See also

- [index_of](index_of.md), [last_index_of](last_index_of.md): the position of an element equal to a value
- [exists](exists.md): checks whether a predicate accepts some element
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
