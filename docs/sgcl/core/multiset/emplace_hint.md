[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...`, as [emplace](emplace.md) does; the hint is ignored. A hash table has
no place for a hint to point at: the bucket of the key decides, and the element goes in front of those with the
same key.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator, ignored |
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
    multiset<string> s = {"zzz"};
    auto it = s.emplace_hint(s.end(), 3, 'z');
    println("{} {} {}", *it, s.size(), s.count("zzz"));
}
```

Output:

```text
zzz 2 2
```

## See also

- [emplace](emplace.md): the same without a hint
- [insert](insert.md): inserts an element, with or without a hint
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
