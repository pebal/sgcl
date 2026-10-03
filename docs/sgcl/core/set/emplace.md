[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` unless its key is there. As in `std`, the element is built in a new
node before it is looked up, since the key is known only then; when the key is there, the new element is
destroyed and the node dropped. A single argument of the type `Key` is looked up first and inserted as
[insert](insert.md) inserts it, with nothing built for a key that is there.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of the element |

## Return value

The element with the key, and `true` when it was inserted. `pair` is the alias of `std::pair`
([aliases](../aliases.md)).

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
    set<string> s;
    auto [it, fresh] = s.emplace(3, 'x');
    println("{} {}", *it, fresh);

    auto [same, again] = s.emplace("xxx");  // built, found, destroyed
    println("{} {} {}", *same, again, s.size());
}
```

Output:

```text
xxx true
xxx false 1
```

## See also

- [insert](insert.md): inserts an element looked up first
- [emplace_hint](emplace_hint.md): the same with a hint
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
