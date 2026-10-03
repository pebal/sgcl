[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::size

```cpp
size_type size() const noexcept;
```

Counts the elements: a walk over the bottom list from the head to the end, counting the nodes that are not
erased.

## Parameters

None.

## Return value

The number of elements the walk found.

## Complexity

Linear in the number of elements.

## Exceptions

None.

## Notes

The map keeps no count: a count would be one more word every insertion and every erasure writes. Under concurrent
insertions and erasures the walk passes the nodes at different moments, so the number is a snapshot of no
particular moment; it is exact once the other threads are quiet. The walk writes nothing.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<int, int> squares;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&squares, t] {
            for (int i : range(100)) {
                int key = i * 4 + t;
                squares.try_emplace(key, key * key);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{}", squares.size());
}
```

Output:

```text
400
```

## See also

- [empty](empty.md): checks whether the map holds an element, without the count
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
