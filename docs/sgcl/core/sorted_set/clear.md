[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node from the tree. The header node and the comparison stay; the
nodes are left to the collector.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the size of the set.

## Exceptions

None.

## Notes

The elements die in `clear`, not when the collector comes: their destructors run before it returns.
Every iterator to an element is invalid afterwards; `end()` stays valid.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    int id;

    ~Noisy() {
        println("~{}", id);
    }

    bool operator<(const Noisy& other) const noexcept {
        return id < other.id;
    }
};

int main() {
    sorted_set<Noisy> set;
    set.emplace(1);
    set.emplace(2);
    auto end = set.end();

    println("clear");
    set.clear();
    println("{} {}", set.size(), end == set.end());
}
```

Output:

```text
clear
~2
~1
0 true
```

## See also

- [erase](erase.md): destroys some elements
- [empty](empty.md): checks whether the set is empty
- [sgcl::sorted_set\<Key, Compare\>](README.md)
