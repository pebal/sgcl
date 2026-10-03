[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the newest element: the order of insertion walked backwards, from the newest
element to the oldest. An empty map gives [rend()](rend.md).

## Parameters

None.

## Return value

`reverse_iterator(end())`, or its `const` form.

## Complexity

Constant.

## Exceptions

None.

## Notes

The reverse iterators are `std::reverse_iterator` over the map's own: each step back is a load of the node's
link to the one before. A map that has never had a bucket array gives a null `rbegin()` equal to `rend()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m;
    m["c"] = 1;
    m["a"] = 2;
    m["b"] = 3;
    m["a"] = 4;

    vector<string> keys;
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        keys.push_back(it->first);
    }
    println("{} {}", keys, m.crbegin()->second);
}
```

Output:

```text
["b", "a", "c"] 3
```

## See also

- [rend, crend](rend.md): the reverse iterator past the oldest element
- [back](back.md): the newest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
