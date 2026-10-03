[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

As [emplace](emplace.md): inserts an element constructed from `a...` unless its key is there, the element built
first and destroyed again when its key is there. `hint` is ignored: a new element goes to the end of the order,
wherever `hint` points.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the map, ignored |
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the element inserted, or to the element already under the key.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the map is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, string> m = {{"a", "first"}};
    auto it = m.emplace_hint(m.begin(), "z", "last");
    println("{} {}", it->first, m);

    it = m.emplace_hint(m.end(), "a", "again");
    println("{}", it->second);
}
```

Output:

```text
z {"a": "first", "z": "last"}
first
```

## See also

- [emplace](emplace.md): the same without a hint
- [try_emplace](try_emplace.md): builds the value only when the key is absent
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
