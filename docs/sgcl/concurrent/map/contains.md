[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::contains

```cpp
bool contains(const Key& key) const noexcept;                    // (1)
template<class K> bool contains(const K& key) const noexcept;    // (2)
```

Checks whether the map holds an element under `key`: the search of [find](find.md), without an iterator.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when an element is under `key`, `false` otherwise.

## Complexity

Constant on average, as [find](find.md).

## Exceptions

None.

## Notes

Wait-free once the key's bucket has its dummy node, as `find`. The answer is of the moment of the search: a thread
that inserts a key unless it is there calls [try_emplace](try_emplace.md), which decides at its own
compare-exchange, rather than `contains` and then an insertion that another thread may have made in between.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{} {}", ports.contains("https"), ports.contains("ftp"));
}
```

Output:

```text
true false
```

## See also

- [find](find.md): an iterator to the element
- [count](count.md): the same answer as a number
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
