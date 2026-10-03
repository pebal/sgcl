[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::rend, crend

```cpp
reverse_iterator rend() noexcept;                 // (1)
const_reverse_iterator rend() const noexcept;     // (2)
const_reverse_iterator crend() const noexcept;    // (3)
```

Returns the reverse iterator past the oldest element, the end of a walk of the order backwards. It is not
dereferenced.

## Parameters

None.

## Return value

`reverse_iterator(begin())`, or its `const` form.

## Complexity

Constant.

## Exceptions

None.

## Notes

`rend()` is made from `begin()`: it changes when the oldest element changes (an erasure of it, an insertion into
an empty map, a [to_front](to_front.md)). Taken after the change, it is the end of the walk again.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    vector<string> names;
    for (auto it = m.crbegin(); it != m.crend(); ++it) {
        names.push_back(it->second);
    }
    println("{}", names);

    auto oldest = std::prev(m.rend());
    println("{}", oldest->second);
}
```

Output:

```text
["three", "two", "one"]
one
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the newest element
- [front](front.md): the oldest element
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
