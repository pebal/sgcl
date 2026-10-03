[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed in place from `a...`, as [emplace](emplace.md) does; the hint is ignored.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the multimap, not used |
| `a` | the arguments of the element's constructor |

## Return value

An iterator to the inserted element.

## Complexity

Constant on average, linear in the size when every key falls into one bucket; a growth relinks every node,
amortized constant.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted and the multimap is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, string> m;
    auto it = m.emplace_hint(m.end(), "z", "last");
    it = m.emplace_hint(it, "z", "again");
    println("{} {}", it->second, m.count("z"));
}
```

Output:

```text
again 2
```

## See also

- [emplace](emplace.md): the same, without a hint
- [insert](insert.md): inserts elements or nodes
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
