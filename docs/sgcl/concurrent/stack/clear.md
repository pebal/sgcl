[sgcl](../../README.md) › [concurrent](../README.md) › [stack](../stack.md)

# sgcl::concurrent::stack\<T\>::clear

```cpp
void clear() noexcept;
```

Takes every element off the stack at once: the head is swung to null with a compare-exchange, and the elements of
the nodes it held are destroyed on the calling thread, from the top down. The nodes are the collector's.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements, with one compare-exchange on the head.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange: `clear` takes exactly the elements on the stack at that
moment, and an element pushed after it stays on the stack. A thread that popped an element before the exchange
has it; no element is both popped and destroyed by `clear`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::stack<string> lines;
    lines.push("a");
    lines.push("b");
    lines.push("c");

    lines.clear();
    println("{} {}", lines.empty(), lines.size());
}
```

Output:

```text
true 0
```

## See also

- [try_pop](try_pop.md): takes one element
- [sgcl::concurrent::stack\<T\>](../stack.md)
