[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::emplace

```cpp
template<class... A>
iterator emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` in a new node, as `value_type(std::forward<A>(a)...)`, and links it
after the elements with an equivalent key.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the inserted element.

## Complexity

Logarithmic in the size of the multimap.

## Exceptions

What the constructor of `value_type` from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is inserted.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <tuple>
#include <utility>

using namespace sgcl;

int main() {
    sorted_multimap<string, string> m;
    m.emplace("k", "v");
    auto it = m.emplace(std::piecewise_construct, std::forward_as_tuple("k"),
                        std::forward_as_tuple(3, 'x'));  // a second "k"
    println("{}", *it);
    println("{}", m);
}
```

Output:

```text
("k", "xxx")
{"k": "v", "k": "xxx"}
```

## See also

- [emplace_hint](emplace_hint.md): constructs an element in place, with a hint
- [insert](insert.md): inserts elements or nodes
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
