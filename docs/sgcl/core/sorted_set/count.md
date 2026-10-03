[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements equivalent to `key`: 1 or 0, the keys of a set being unique.

- (2) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Takes part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements to count |

## Return value

The number of elements with the key, 1 or 0.

## Complexity

Logarithmic in the size of the set.

## Exceptions

- (1) None.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept, or when `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<string> colours = {"red", "green", "red"};
    println("{} {}", colours.count("red"), colours.count("blue"));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether a key is there
- [equal_range](equal_range.md): the elements with a key
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
