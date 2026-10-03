[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last element: an iterator that holds no node. It is what an increment from the
last element of the list gives, and what [find](find.md) returns for a key that is not there.

## Parameters

None.

## Return value

The iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The end of the list is the same for every thread and never moves: an element inserted after the last one is
reached by an increment, and an `end()` taken before still compares equal to the end of any later pass. It may not
be dereferenced, nor passed to [erase](erase.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<string, int> ports = {{"http", 80}, {"https", 443}};

    auto it = ports.find("ftp");
    println("{}", it == ports.end());

    it = ports.find("https");
    println("{} {}", it != ports.end(), it->second);
}
```

Output:

```text
true
true 443
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [find](find.md): returns `end()` for a missing key
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
