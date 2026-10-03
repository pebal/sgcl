[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex/README.md) › [guard](README.md)

# sgcl::async::shared_mutex::guard::guard

```cpp
explicit guard(shared_mutex& m) noexcept;    // (1)
guard(guard&& o) noexcept;                   // (2)
guard(const guard&) = delete;                // (3)
```

1. Takes over the writer's lock of `m`, which the caller holds: the guard locks nothing, and gives the lock back
   when it is destroyed. The form of `std::lock_guard(m, std::adopt_lock)`; a guard that locks is made by
   [scoped_lock](../shared_mutex/scoped_lock.md).
2. Takes the lock `o` holds; `o` is left empty, holding nothing.
3. A guard is not copyable: one lock taken once has one guard.

## Parameters

| Parameter | Description |
|---|---|
| `m` | a shared mutex whose writer's lock the caller holds |
| `o` | the guard whose lock is taken |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

int main() {
    async::shared_mutex m;
    m.lock();
    {
        async::shared_mutex::guard first(m);  // the writer's lock is taken already
        async::shared_mutex::guard second = std::move(first);
        println("{}", m.try_lock_shared());
    }
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

- [scoped_lock](../shared_mutex/scoped_lock.md): locks and makes the guard
- [release](release.md): gives the lock up without giving it back
- [sgcl::async::shared_mutex::guard](README.md)
