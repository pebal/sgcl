[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Erases every element there is at the time of the walk: from the head along the list, each element erased as
[erase](erase.md) erases it, marked and unlinked.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements, plus the dummies of the buckets used.

## Exceptions

None.

## Notes

Under concurrent insertions `clear` takes what it reaches: an element inserted while it runs may be erased or left,
and the map may hold elements when it returns. Every erasure of it is lock-free. The elements are destroyed by the
collector, as an erased element is; the bucket array and the dummies of the buckets stay as they were.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, int> squares;
    for (int i : range(100)) {
        squares.try_emplace(i, i * i);
    }

    squares.clear();
    println("{} {} {}", squares.empty(), squares.size(), squares.bucket_count());
}
```

Output:

```text
true 0 128
```

## See also

- [erase](erase.md): erases one element
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
