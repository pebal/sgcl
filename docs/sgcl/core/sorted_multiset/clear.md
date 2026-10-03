[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::clear

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

Linear in the size of the multiset.

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
    sorted_multiset<Noisy> multiset;
    multiset.emplace(1);
    multiset.emplace(1);
    auto end = multiset.end();

    println("clear");
    multiset.clear();
    println("{} {}", multiset.size(), end == multiset.end());
}
```

Output:

```text
clear
~1
~1
0 true
```

## See also

- [erase](erase.md): destroys some elements
- [empty](empty.md): checks whether the multiset is empty
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
