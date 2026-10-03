[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](README.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last element: an iterator that holds no node. It is what `++` gives after the last
element, and what a lookup gives when it finds nothing; it may not be dereferenced.

## Parameters

None.

## Return value

The iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

`end()` reads nothing of the map, so it is the same whatever the other threads do: an iterator that walked off the
last element compares equal to it, even when other threads have inserted elements after that one meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};

    println("{}", ports.find("ftp") == ports.end());

    int count = 0;
    for (auto it = ports.cbegin(); it != ports.cend(); ++it) {
        ++count;
    }
    println("{}", count);
}
```

Output:

```text
true
2
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](README.md)
