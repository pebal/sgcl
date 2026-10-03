[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
iterator emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` in a new node, linked in front of the elements with the same key, if
any. A single argument of the type `Key` is inserted as [insert](insert.md) inserts it.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of the element |

## Return value

The inserted element.

## Complexity

Constant on average, the walk of one bucket; a growth of the table relinks every node, amortized constant.

## Exceptions

What the constructor of the element throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the multiset is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s;
    s.emplace(3, 'x');
    auto it = s.emplace(3, 'x');
    println("{} {} {}", *it, s.count("xxx"), s.find("xxx") == it);
}
```

Output:

```text
xxx 2 true
```

## See also

- [insert](insert.md): inserts an element
- [emplace_hint](emplace_hint.md): the same with a hint
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
