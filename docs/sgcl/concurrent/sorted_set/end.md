[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last key: an iterator that holds no node. It is what `++` gives after the last key,
and what a lookup gives when it finds nothing; it may not be dereferenced.

## Parameters

None.

## Return value

The iterator past the last key.

## Complexity

Constant.

## Exceptions

None.

## Notes

`end()` reads nothing of the set, so it is the same whatever the other threads do: an iterator that walked off the
last key compares equal to it, even when other threads have inserted keys after that one meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> primes = {2, 3, 5, 7};

    println("{}", primes.find(4) == primes.end());
    println("{}", primes.upper_bound(7) == primes.cend());
}
```

Output:

```text
true
true
```

## See also

- [begin, cbegin](begin.md): the iterator to the first key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](../sorted_set.md)
