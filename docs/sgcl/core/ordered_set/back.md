[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::back

```cpp
const value_type& back() const noexcept;
```

Returns a reference to the newest element: the last of the order, the one before [end()](end.md). The set must
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
an element since. An insertion of an element that is there already does not make it the newest.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {2, 1};
    s.insert(3);
    println("{}", s.back());

    s.insert(2);  // there: the place stays
    println("{}", s.back());

    s.to_back(s.find(2));
    println("{} {}", s.back(), s);
}
```

Output:

```text
3
3
2 {1, 3, 2}
```

## See also

- [front](front.md): the oldest element
- [rbegin, crbegin](rbegin.md): a reverse iterator to the newest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
