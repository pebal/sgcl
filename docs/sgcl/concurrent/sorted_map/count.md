[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::count

```cpp
/*(1)*/ size_type count(const Key& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept;
```

Returns the number of elements whose key is equivalent to `key`: 1 or 0, as the keys are unique. It is
[contains](contains.md) as a number, the form `std::map` has.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements to count |

## Return value

1 when the map holds an element under `key`, 0 otherwise.

## Complexity

Logarithmic in the size of the map, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing; the answer is of the moment the search reached the bottom list.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, string> names = {{1, "Ada"}, {2, "Grace"}};
    println("{} {}", names.count(1), names.count(3));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): the same question with a `bool` answer
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
