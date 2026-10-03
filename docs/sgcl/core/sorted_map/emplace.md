[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` in a new node, as `value_type(std::forward<A>(a)...)`, unless the map
holds its key. The element is built before its place is known, as in `std`: when the key is already there, the
new element is destroyed again and the existing one returned, and the node is left to the collector.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

A pair of an iterator and a `bool`: the inserted element and `true`, or the element already under the key and
`false`.

## Complexity

Logarithmic in the size of the map.

## Exceptions

What the constructor of `value_type` from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted.

## Notes

[try_emplace](try_emplace.md) looks the key up first and builds nothing when it is there; `emplace` is for an
element whose key is known only once it is built.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <tuple>
#include <utility>

using namespace sgcl;

int main() {
    sorted_map<string, string> m;
    m.emplace("k", "v");
    m.emplace(std::piecewise_construct, std::forward_as_tuple("p"), std::forward_as_tuple(3, 'x'));
    auto [it, inserted] = m.emplace("k", "w");  // built, then destroyed: "k" is taken
    println("{} {}", *it, inserted);
    println("{}", m);
}
```

Output:

```text
("k", "v") false
{"k": "v", "p": "xxx"}
```

## See also

- [emplace_hint](emplace_hint.md): constructs an element in place, with a hint
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert](insert.md): inserts elements or nodes
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
