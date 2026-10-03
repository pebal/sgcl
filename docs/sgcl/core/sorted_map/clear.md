[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::clear

```cpp
void clear() noexcept;
```

Destroys every element at once, from the largest key down, and unlinks every node; the nodes' memory is reclaimed
by the collector later. The header stays, so an iterator taken from [end()](end.md) remains equal to `end()`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the size of the map.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Loud {
    int id;
    ~Loud() {
        println("~Loud {}", id);
    }
};

int main() {
    sorted_map<int, Loud> m;
    m.try_emplace(1, 1);
    m.try_emplace(2, 2);
    auto end = m.end();

    m.clear();
    println("{} {}", m.empty(), end == m.end());
}
```

Output:

```text
~Loud 2
~Loud 1
true true
```

## See also

- [erase](erase.md): erases elements
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
