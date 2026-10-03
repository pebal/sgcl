[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements in the multimap.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant: the count is stored.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> positions;
    int position = 0;
    for (string word : {"to", "be", "or", "not", "to", "be"}) {
        positions.emplace(word, position++);
    }
    println("{} {}", positions.size(), positions.count("to"));
}
```

Output:

```text
6 2
```

## See also

- [empty](empty.md): checks whether the multimap is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
