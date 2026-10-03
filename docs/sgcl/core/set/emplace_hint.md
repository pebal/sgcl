[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` unless its key is there, as [emplace](emplace.md) does; the hint is
ignored. A hash table has no place for a hint to point at: the bucket of the key decides.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator, ignored |
| `a` | the arguments of the constructor of the element |

## Return value

The element with the key, inserted or already there.

## Complexity

Constant on average, the walk of one bucket; a growth of the table relinks every node, amortized constant.

## Exceptions

What the constructor of the element throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the set is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s = {"zzz"};
    auto it = s.emplace_hint(s.end(), 3, 'a');
    println("{} {}", *it, s.size());

    it = s.emplace_hint(s.begin(), "zzz");
    println("{} {}", *it, s.size());
}
```

Output:

```text
aaa 2
zzz 2
```

## See also

- [emplace](emplace.md): the same, and whether the element was inserted
- [insert](insert.md): inserts an element, with or without a hint
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
