[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::front

```cpp
reference front() noexcept;                // (1)
const_reference front() const noexcept;    // (2)
```

Returns a reference to the first element. The deque must not be empty.

## Parameters

None.

## Return value

A reference to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

On an empty deque the call is undefined behaviour, as with `std::deque`. The reference stays valid across a push
at either end; [pop_front](pop_front.md) destroys the element it refers to.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> tasks = {"read", "write"};
    tasks.push_front("open");
    println("{}", tasks.front());

    tasks.front() = "connect";
    println("{}", tasks);
}
```

Output:

```text
open
["connect", "read", "write"]
```

## See also

- [back](back.md): access the last element
- [push_front](push_front.md), [pop_front](pop_front.md): insert, remove the first element
- [sgcl::deque\<T\>](../deque.md)
