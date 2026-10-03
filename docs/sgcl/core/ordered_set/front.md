[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::front

```cpp
const value_type& front() const noexcept;
```

Returns a reference to the oldest element: the first of the order, the one [begin()](begin.md) addresses. The
set must not be empty; nothing is checked. The reference is `const`: an element is never changed in place.

## Parameters

None.

## Return value

A reference to the first element of the order.

## Complexity

Constant.

## Exceptions

None.

## Notes

The oldest element is the one inserted first, unless [to_front](to_front.md) or [to_back](to_back.md) has moved
an element since.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s = {"b", "a", "c"};
    println("{}", s.front());

    s.to_back(s.begin());
    println("{} {}", s.front(), s);
}
```

Output:

```text
b
a {"a", "c", "b"}
```

## See also

- [back](back.md): the newest element
- [begin, cbegin](begin.md): an iterator to the oldest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
