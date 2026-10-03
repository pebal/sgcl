[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::emplace_hint

```cpp
template<class... A>
iterator emplace_hint(const_iterator hint, A&&... a)
    noexcept(std::is_nothrow_constructible_v<value_type, A&&...>);
```

As [emplace](emplace.md): inserts an element constructed from `a...` unless an equal one is there, the element
built first and destroyed again when it is there. `hint` is ignored: a new element goes to the end of the order,
wherever `hint` points.

## Parameters

| Parameter | Description |
|---|---|
| `hint` | an iterator into the set, ignored |
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the element inserted, or to the equal element already there.

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
    ordered_set<string> s = {"first"};
    auto it = s.emplace_hint(s.begin(), "last");
    println("{} {}", *it, s);

    it = s.emplace_hint(s.end(), "first");
    println("{}", it == s.begin());
}
```

Output:

```text
last {"first", "last"}
true
```

## See also

- [emplace](emplace.md): the same without a hint
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
