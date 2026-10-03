[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::contains

```cpp
bool contains(const Key& key) const noexcept;                    // (1)
template<class K> bool contains(const K& key) const noexcept;    // (2)
```

Checks whether the map holds an element whose key is equivalent to `key`, with the search of [find](find.md).

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when the map holds an element under `key`, `false` otherwise.

## Complexity

Logarithmic in the size of the map, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing. The answer is of the moment the search reached the bottom list: another
thread may insert or erase the key right after. A thread that wants the element asks [find](find.md), whose
iterator holds it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    concurrent::sorted_map<string, int> ports = {{"http", 80}, {"ssh", 22}};
    println("{} {}", ports.contains("ssh"), ports.contains("ftp"));

    std::string_view name = "http";
    println("{}", ports.contains(name));  // no string made for the search
}
```

Output:

```text
true false
true
```

## See also

- [find](find.md): finds the element
- [count](count.md): the number of elements under a key
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
