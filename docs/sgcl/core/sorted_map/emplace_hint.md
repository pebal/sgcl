[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` in a new node, as [emplace](emplace.md) does, with `hint` the element
the new one should go right before. When the key is already there, the new element is destroyed again and the
existing one returned. The hint is used when the key belongs right before it: one or two comparisons instead of a
search, and an append in sorted order at `end()` costs one.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | the element before which the new one should go; `end()` for the end |
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the inserted element, or to the one already under the key.

## Complexity

Amortized constant when the key belongs right before `hint`, otherwise logarithmic in the size of the map.

## Exceptions

What the constructor of `value_type` from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<int, string> m;
    for (int i : range(1, 6)) {
        m.emplace_hint(m.end(), i, string(i, '*'));  // sorted input at end(): one comparison each
    }
    println("{}", m);

    auto it = m.emplace_hint(m.begin(), 3, "three");  // 3 is taken
    println("{}", it->second);
}
```

Output:

```text
{1: "*", 2: "**", 3: "***", 4: "****", 5: "*****"}
***
```

## See also

- [emplace](emplace.md): constructs an element in place
- [insert](insert.md): inserts elements or nodes, with or without a hint
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
