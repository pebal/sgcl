[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::back

```cpp
value_type& back() noexcept;                // (1)
const value_type& back() const noexcept;    // (2)
```

Returns a reference to the newest element: the last of the order, the one before [end()](end.md). The map must
not be empty; nothing is checked.

## Parameters

None.

## Return value

A reference to the last element of the order.

## Complexity

Constant.

## Exceptions

None.

## Notes

The newest element is the one inserted last, unless [to_back](to_back.md) or [to_front](to_front.md) has moved
an element since. An insertion of a key that is there already does not make its element the newest.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"b", 2}, {"a", 1}};
    m["c"] = 3;
    println("{} {}", m.back().first, m.back().second);

    m["b"] = 4;  // present: the place stays
    println("{}", m.back().first);

    m.to_back(m.find("b"));
    println("{} {}", m.back().first, m);
}
```

Output:

```text
c 3
c
b {"a": 1, "c": 3, "b": 4}
```

## See also

- [front](front.md): the oldest element
- [rbegin, crbegin](rbegin.md): a reverse iterator to the newest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
