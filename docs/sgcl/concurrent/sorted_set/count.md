[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::count

```cpp
size_type count(const Key& key) const noexcept;                    // (1)
template<class K> size_type count(const K& key) const noexcept;    // (2)
```

Returns the number of keys of the set equivalent to `key`: 1 or 0, as the keys are unique. It is
[contains](contains.md) as a number, the form `std::set` has.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

1 when the set holds a key equivalent to `key`, 0 otherwise.

## Complexity

Logarithmic in the size of the set, expected.

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
    concurrent::sorted_set<int> primes = {2, 3, 5, 7};
    println("{} {}", primes.count(5), primes.count(6));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): the same question with a `bool` answer
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
