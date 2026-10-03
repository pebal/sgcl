[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements under `key`: 0 or 1, the keys being unique.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

The number of elements under `key`, 0 or 1.

## Complexity

Logarithmic in the size of the map.

## Exceptions

- (1) None.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function object
  of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> m = {{"a", 1}, {"b", 2}};
    println("{} {}", m.count("a"), m.count("z"));  // no string is built for a literal
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether the map holds a key
- [find](find.md): finds the element under a key
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
