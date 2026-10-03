[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::clear

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

Linear in the size of the multimap.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Loud {
    char id;
    ~Loud() {
        println("~Loud {}", id);
    }
};

int main() {
    sorted_multimap<int, Loud> m;
    m.emplace(1, 'a');
    m.emplace(1, 'b');
    m.emplace(2, 'c');

    m.clear();
    println("{}", m.empty());
}
```

Output:

```text
~Loud c
~Loud b
~Loud a
true
```

## See also

- [erase](erase.md): erases elements
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
