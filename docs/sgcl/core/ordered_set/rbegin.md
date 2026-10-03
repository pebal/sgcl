[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::rbegin, crbegin

```cpp
/*(1)*/ const_reverse_iterator rbegin() const noexcept;
/*(2)*/ const_reverse_iterator crbegin() const noexcept;
```

Returns a reverse iterator to the newest element: the order of insertion walked backwards, from the newest
element to the oldest. An empty set gives [rend()](rend.md). Both forms are `const`, the elements being
read-only.

## Parameters

None.

## Return value

`const_reverse_iterator(end())`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The reverse iterators are `std::reverse_iterator` over the set's own: each step back is a load of the node's
link to the one before. A set that has never had a bucket array gives a null `rbegin()` equal to `rend()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s = {"c", "a", "b"};
    s.insert("a");

    vector<string> keys;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        keys.push_back(*it);
    }
    println("{} {}", keys, *s.crbegin());
}
```

Output:

```text
["b", "a", "c"] b
```

## See also

- [rend, crend](rend.md): the reverse iterator past the oldest element
- [back](back.md): the newest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
