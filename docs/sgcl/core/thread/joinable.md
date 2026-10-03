[sgcl](../../README.md) › [core](../README.md) › [thread](README.md)

# sgcl::thread::joinable

```cpp
bool joinable() const noexcept;
```

Checks whether the object stands for a thread of execution that has not been joined or detached. A thread that
has finished its function but has not been joined is still joinable.

## Parameters

None.

## Return value

`true` when the object stands for a thread, `false` for a default-constructed object, one moved from, one
joined and one detached.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    thread none;
    thread worker([] {});
    println("{} {}", none.joinable(), worker.joinable());
    worker.join();
    println("{}", worker.joinable());
}
```

Output:

```text
false true
false
```

## See also

- [join](join.md), [detach](detach.md): what makes a thread no longer joinable
- [sgcl::thread](README.md)
