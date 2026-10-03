[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed in place from `a...`, unless its key is in the map. As in `std`, the element is
built in a new node before the key is looked up: when the key is there, the new element is destroyed at once
and the element already there is returned. A single argument of the type `value_type` is looked up first and
inserted as [insert](insert.md) inserts it, built only when the key is absent.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the element's constructor: a key and a value, a pair, or `std::piecewise_construct` and two tuples |

## Return value

An iterator to the element under the key and `true` when it was inserted, or to the element already there and
`false`.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted and the map is as it was.

## Notes

The node of an element destroyed on a taken key is left to the collector.
[try_emplace](try_emplace.md) looks the key up before it builds anything.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <tuple>
#include <utility>

using namespace sgcl;

int built = 0;

struct Value {
    string text;
    Value(const char* t) : text(t) { ++built; }
};

int main() {
    map<string, Value> m;
    m.emplace("k", "v");
    auto [it, inserted] = m.emplace("k", "w");  // built, then destroyed: the key is taken
    println("{} {} {}", it->second.text, inserted, built);

    m.emplace(std::piecewise_construct, std::forward_as_tuple("p"), std::forward_as_tuple("xxx"));
    println("{} {}", m.size(), m.at("p").text);
}
```

Output:

```text
v false 2
2 xxx
```

## See also

- [emplace_hint](emplace_hint.md): the same, with a hint
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [insert](insert.md): inserts elements or nodes
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
