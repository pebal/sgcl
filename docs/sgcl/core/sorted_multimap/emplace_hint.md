[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` in a new node, as [emplace](emplace.md) does, with `hint` the element
the new one should go right before. The hint is used when the element belongs right before it, among its
equivalents too: one or two comparisons instead of a search, and an append in sorted order at `end()` costs one.
A hint that does not fit is ignored, and the element goes after its equivalents.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | the element before which the new one should go; `end()` for the end |
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the inserted element.

## Complexity

Amortized constant when the element belongs right before `hint`, otherwise logarithmic in the size of the
multimap.

## Exceptions

What the constructor of `value_type` from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{1, "b"}, {2, "c"}};
    m.emplace_hint(m.begin(), 1, "a");  // before the 1 already there
    m.emplace_hint(m.end(), 3, "last");
    println("{}", m);
}
```

Output:

```text
{1: "a", 1: "b", 2: "c", 3: "last"}
```

## See also

- [emplace](emplace.md): constructs an element in place
- [insert](insert.md): inserts elements or nodes, with or without a hint
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
