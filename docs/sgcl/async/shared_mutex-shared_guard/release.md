[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md) › [shared_guard](../shared_mutex-shared_guard.md)

# sgcl::async::shared_mutex::shared_guard::release

```cpp
shared_mutex* release() noexcept;
```

Gives the reader's lock up without giving it back, as `std::shared_lock::release` does: the guard is left empty,
its destructor does nothing, and the caller holds the lock and gives it back with
[unlock_shared](../shared_mutex/unlock_shared.md).

## Parameters

None.

## Return value

The address of the shared mutex the guard held a lock of, or a null pointer when it held nothing.

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
        auto guard = m.scoped_lock_shared().wait();
        held = guard.release();
    }
    println("{}", m.try_lock());  // the reader's lock is still held
    held->unlock_shared();
    println("{}", m.try_lock());
    m.unlock();
}
```

Output:

```text
false
true
```

## See also

- [owner](owner.md): the shared mutex, kept by the guard
- [(constructor)](shared_mutex-shared_guard.md): a guard that takes over a reader's lock
- [unlock_shared](../shared_mutex/unlock_shared.md): gives the lock back
- [sgcl::async::shared_mutex::shared_guard](../shared_mutex-shared_guard.md)
