[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed in place from `a...`, unless its key is in the map, as [emplace](emplace.md)
does; the hint is ignored.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, not used |
| `a` | the arguments of the element's constructor |

## Return value

An iterator to the element under the key, inserted or already there.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted and the map is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, string> m;
    auto it = m.emplace_hint(m.end(), "z", "last");
    println("{} {}", it->first, it->second);

    it = m.emplace_hint(m.begin(), "z", "again");  // the key is taken
    println("{} {} {}", it->first, it->second, m.size());
}
```

Output:

```text
z last
z last 1
```

## See also

- [emplace](emplace.md): the same, telling whether the element was inserted
- [try_emplace](try_emplace.md): builds the element only when the key is absent
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
