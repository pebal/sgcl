[sgcl](../../README.md) › [core](../README.md) › [multimap](README.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
iterator emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed in place from `a...`, in a new node, and links it in front of the elements with an
equivalent key, if there are any.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the element's constructor: a key and a value, a pair, or `std::piecewise_construct` and two tuples |

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
#include <tuple>
#include <utility>

using namespace sgcl;

int main() {
    multimap<string, string> m;
    m.emplace("k", "v");
    auto it = m.emplace(std::piecewise_construct, std::forward_as_tuple("k"),
                        std::forward_as_tuple(3, 'x'));
    println("{} {} {}", it->second, m.count("k"), m.find("k") == it);
}
```

Output:

```text
xxx 2 true
```

## See also

- [emplace_hint](emplace_hint.md): the same, with a hint
- [insert](insert.md): inserts elements or nodes
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](README.md)
