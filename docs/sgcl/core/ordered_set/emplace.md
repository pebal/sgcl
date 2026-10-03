[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::emplace

```cpp
template<class... A>
pair<iterator, bool> emplace(A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

Inserts an element constructed from `a...` unless an equal one is there, as `std::unordered_set::emplace` does.
The element is built in a new node first and looked up after: when it is there, the new element is destroyed at
once, the node is left to the collector, and the element there is returned, in its place. A new element goes to
the end of the order.

A single argument of the type `Key` is looked up before anything is built, as [insert](insert.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

The element and `true`, or the equal element already there and `false`.

## Complexity

Constant on average, linear in `size()` in the worst case; amortized over the growths of the table.

## Exceptions

What the construction of the element from `a...` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the set is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s;
    s.emplace(3, 'x');
    s.emplace("abc");

    auto [it, inserted] = s.emplace("xxx");  // built, then destroyed: "xxx" is there
    println("{} {} {}", *it, inserted, s);
}
```

Output:

```text
xxx false {"xxx", "abc"}
```

## See also

- [emplace_hint](emplace_hint.md): the same with a hint
- [insert](insert.md): inserts a built element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
