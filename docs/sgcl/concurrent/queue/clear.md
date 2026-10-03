[sgcl](../../README.md) › [concurrent](../README.md) › [queue](README.md)

# sgcl::concurrent::queue\<T\>::clear

```cpp
void clear() noexcept;
```

Pops every element there is: [try_pop](try_pop.md) until it finds the queue empty, each element destroyed on the
calling thread.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements.

## Exceptions

None. The function is `noexcept`: a move constructor of `T` that throws ends the program.

## Notes

Under concurrent pushes `clear` takes what it reaches: an element pushed while it runs may be taken or left, and
the queue may hold elements when it returns. Every pop of it is lock-free.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<string> lines;
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
- [sgcl::concurrent::queue\<T\>](README.md)
