[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md) › [guard](../shared_mutex-guard.md)

# sgcl::async::shared_mutex::guard::release

```cpp
shared_mutex* release() noexcept;
```

Gives the writer's lock up without giving it back, as `std::unique_lock::release` does: the guard is left empty, its
destructor does nothing, and the caller holds the lock and gives it back with [unlock](../shared_mutex/unlock.md).

## Parameters

None.

## Return value

The address of the shared mutex the guard held the lock of, or a null pointer when it held nothing.

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
    async::shared_mutex m;
    async::shared_mutex* held = nullptr;
    {
        auto guard = m.scoped_lock().wait();
        held = guard.release();
    }
    println("{}", m.try_lock_shared());  // the writer's lock is still held
    held->unlock();
    println("{}", m.try_lock_shared());
    m.unlock_shared();
}
```

Output:

```text
false
true
```

## See also

- [owner](owner.md): the shared mutex, kept by the guard
- [(constructor)](shared_mutex-guard.md): a guard that takes over the writer's lock
- [unlock](../shared_mutex/unlock.md): gives the lock back
- [sgcl::async::shared_mutex::guard](../shared_mutex-guard.md)
