[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements under `key`: the run [equal_range](equal_range.md) finds, walked and counted.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

The number of elements under `key`.

## Complexity

Logarithmic in the size of the multimap, plus linear in the number of elements under `key`.

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
    sorted_multimap<string, int> m = {{"a", 1}, {"a", 2}, {"b", 3}};
    println("{} {} {}", m.count("a"), m.count("b"), m.count("z"));  // no string built for a literal
}
```

Output:

```text
2 1 0
```

## See also

- [equal_range](equal_range.md): the range of the elements under a key
- [contains](contains.md): checks whether the multimap holds a key
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
