[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::size

```cpp
size_type size() const noexcept;
```

Counts the keys: a walk over the bottom list from the head to the end, counting the nodes that are not erased.

## Parameters

None.

## Return value

The number of keys the walk found.

## Complexity

Linear in the number of keys.

## Exceptions

None.

## Notes

The set keeps no count: a count would be one more word every insertion and every erasure writes. Under concurrent
insertions and erasures the walk passes the nodes at different moments, so the number is a snapshot of no
particular moment; it is exact once the other threads are quiet. The walk writes nothing.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> seen;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&seen] {
            for (int i : range(100)) {
                seen.insert(i);  // the same 100 keys from every thread
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{}", seen.size());
}
```

Output:

```text
100
```

## See also

- [empty](empty.md): checks whether the set holds a key, without the count
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
