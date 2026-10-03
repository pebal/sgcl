[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::rend, crend

```cpp
const_reverse_iterator rend() const noexcept;     // (1)
const_reverse_iterator crend() const noexcept;    // (2)
```

Returns the reverse iterator past the oldest element, the end of a walk of the order backwards. It is not
dereferenced.

## Parameters

None.

## Return value

`const_reverse_iterator(begin())`.

## Complexity

Constant.

## Exceptions

None.

## Notes

`rend()` is made from `begin()`: it changes when the oldest element changes (an erasure of it, an insertion into
an empty set, a [to_front](to_front.md)). Taken after the change, it is the end of the walk again.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {1, 2, 3};
    vector<int> backwards;
    for (auto it = s.crbegin(); it != s.crend(); ++it) {
        backwards.push_back(*it);
    }
    println("{} {}", backwards, *std::prev(s.rend()));
}
```

Output:

```text
[3, 2, 1] 1
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the newest element
- [front](front.md): the oldest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
