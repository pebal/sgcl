[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md) › [guard](../mutex-guard.md)

# sgcl::async::mutex::guard::guard

```cpp
explicit guard(const mutex& m) noexcept;    // (1)
guard(guard&& o) noexcept;                  // (2)
guard(const guard&) = delete;               // (3)
```

1. Takes over `m`, which the caller has locked: the guard locks nothing, and unlocks `m` when it is destroyed. The
   form of `std::lock_guard(m, std::adopt_lock)`; a guard that locks is made by
   [scoped_lock](../mutex/scoped_lock.md).
2. Takes the mutex `o` holds; `o` is left empty, holding nothing.
3. A guard is not copyable: one mutex locked once has one guard.

## Parameters

| Parameter | Description |
|---|---|
| `m` | a mutex the caller has locked |
| `o` | the guard whose mutex is taken |

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
    async::mutex m;
    m.lock();
    {
        async::mutex::guard first(m);  // m is locked already
        async::mutex::guard second = std::move(first);
        println("{}", m.try_lock());
    }
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

- [scoped_lock](../mutex/scoped_lock.md): locks and makes the guard
- [release](release.md): gives the mutex up without unlocking it
- [sgcl::async::mutex::guard](../mutex-guard.md)
