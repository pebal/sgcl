[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex/README.md) › [guard](README.md)

# sgcl::async::shared_mutex::guard::owner

```cpp
shared_mutex* owner() const noexcept;
```

Returns the shared mutex whose writer's lock the guard holds, which stays locked and held by the guard, as
[mutex::guard::owner](../mutex-guard/owner.md) does. A shared mutex is an object, not a handle, so it is given by its
address. A guard moved from or released holds nothing, and gives nothing.

## Parameters

None.

## Return value

The address of the shared mutex the guard holds the lock of, or a null pointer for a guard that holds nothing.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::shared_mutex a;
    async::shared_mutex b;
    auto guard = a.scoped_lock().wait();
    println("{} {}", guard.owner() == &a, guard.owner() == &b);
    auto moved = std::move(guard);
    println("{}", guard.owner() == nullptr);  // moved from: holds nothing
}
```

Output:

```text
true false
true
```

## See also

- [release](release.md): the lock given up, still held
- [mutex::guard::owner](../mutex-guard/owner.md): the same for the guard of a mutex
- [sgcl::async::shared_mutex::guard](README.md)
