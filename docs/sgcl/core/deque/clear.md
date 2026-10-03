[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::clear

```cpp
void clear() noexcept;
```

Destroys every element and drops the blocks and the map: unlike [sgcl::vector](../vector.md), which keeps its
buffer, nothing is kept for the next push. The deque is empty after, as a deque just constructed.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()`; constant for elements whose destructor is trivial.

## Exceptions

None.

## Notes

The blocks and the map are left to the collector, never freed at once. Called from the destructor of a managed
object dying in a sweep, `clear` destroys nothing: the blocks are garbage of the same sweep and destroy the
elements they still hold themselves.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> lines = {"a", "b", "c"};
    lines.clear();
    println("{} {}", lines.empty(), lines.size());

    lines.push_back("d");  // a new block and a new map
    println("{}", lines);
}
```

Output:

```text
true 0
["d"]
```

## See also

- [erase](erase.md): erases elements at a position or in a range
- [shrink_to_fit](shrink_to_fit.md): drops the spare blocks
- [sgcl::deque\<T\>](../deque.md)
