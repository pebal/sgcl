[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` unless its key is there, as `std::unordered_map::emplace` does. The
element is built in a new node first and its key looked up after: when the key is there, the new element is
destroyed at once, the node is left to the collector, and the element under the key is returned, its value and
its place unchanged. A new element goes to the end of the order.

A single argument of the type `value_type` is looked up before anything is built, as [insert](insert.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from: a key and a value, a pair, or `std::piecewise_construct` and two tuples |

## Return value

The element and `true`, or the element already under the key and `false`.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Notes

[try_emplace](try_emplace.md) looks the key up first and builds nothing when it is there; it is the one to use
when the value is expensive to build or is not to be touched on a duplicate.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <tuple>

using namespace sgcl;

int main() {
    ordered_map<string, string> m;
    m.emplace("k", "v");
    m.emplace(std::piecewise_construct, std::forward_as_tuple("p"), std::forward_as_tuple(3, 'x'));

    auto [it, inserted] = m.emplace("k", "w");  // built, then destroyed: "k" is there
    println("{} {} {}", it->second, inserted, m);
}
```

Output:

```text
v false {"k": "v", "p": "xxx"}
```

## See also

- [emplace_hint](emplace_hint.md): the same with a hint
- [try_emplace](try_emplace.md): builds the value only when the key is absent
- [insert](insert.md): inserts a built element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
