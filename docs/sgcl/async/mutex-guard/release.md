[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex/README.md) › [guard](README.md)

# sgcl::async::mutex::guard::release

```cpp
optional<mutex> release() noexcept;
```

Gives the mutex up without unlocking it, as `std::unique_lock::release` does: the guard is left empty, its
destructor does nothing, and the caller holds the lock and unlocks it with [unlock](../mutex/unlock.md), here or
elsewhere, on any thread or task.

## Parameters

None.

## Return value

A handle of the mutex the guard held, locked; `nullopt` for a guard that held nothing (moved from, or released
before), since there is no mutex without a state.

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
    async::mutex m;
    optional<async::mutex> held;
    {
        auto guard = m.scoped_lock().wait();
        held = guard.release();
        println("{}", guard.release().has_value());  // released: holds nothing
    }
    println("{}", m.try_lock());  // still locked after the guard
    held->unlock();
    println("{}", m.try_lock());
    m.unlock();
}
```

Output:

```text
false
false
true
```

## See also

- [owner](owner.md): the mutex, kept by the guard
- [(constructor)](mutex-guard.md): a guard that takes over a locked mutex
- [sgcl::async::mutex::guard](README.md)
