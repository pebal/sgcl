[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::deque\<T\>::shrink_to_fit

```cpp
void shrink_to_fit() noexcept;
```

Replaces the map by one holding exactly the blocks in use: the spare blocks go. When the map holds only the
blocks in use already, nothing happens. An empty deque drops everything, as [clear](clear.md) does.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of blocks in use; no element is touched.

## Exceptions

None.

## Notes

The elements stay in their blocks, so references to them stay valid; iterators do not, when the map is replaced.
The old map and the spare blocks are left to the collector, never freed at once.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<int> d;
    for (int i : range(10000)) {
        d.push_back(i);
    }
    while (d.size() > 3) {
        d.pop_front();
    }

    int& first = d.front();
    d.shrink_to_fit();  // the spare blocks go, the elements stay where they are
    println("{} {}", d, first);
}
```

Output:

```text
[9997, 9998, 9999] 9997
```

## See also

- [clear](clear.md): destroys every element, drops the blocks and the map
- [pop_front](pop_front.md), [pop_back](pop_back.md): remove an element, keeping an emptied block as the spare
- [sgcl::deque\<T\>](README.md)
