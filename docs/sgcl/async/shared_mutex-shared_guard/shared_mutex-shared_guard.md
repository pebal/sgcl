[sgcl](../../README.md) › [async](../README.md) › [shared_mutex](../shared_mutex.md) › [shared_guard](../shared_mutex-shared_guard.md)

# sgcl::async::shared_mutex::shared_guard::shared_guard

```cpp
/*(1)*/ explicit shared_guard(shared_mutex& m) noexcept;
/*(2)*/ shared_guard(shared_guard&& o) noexcept;
/*(3)*/ shared_guard(const shared_guard&) = delete;
```

1. Takes over a reader's lock of `m` that the caller holds: the guard locks nothing, and gives the lock back when
   it is destroyed. The form of `std::shared_lock(m, std::adopt_lock)`; a guard that locks is made by
   [scoped_lock_shared](../shared_mutex/scoped_lock_shared.md).
2. Takes the lock `o` holds; `o` is left empty, holding nothing.
3. A guard is not copyable: one lock taken once has one guard.

## Parameters

| Parameter | Description |
|---|---|
| `m` | a shared mutex of which the caller holds a reader's lock |
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
    m.lock_shared();
    {
        async::shared_mutex::shared_guard first(m);  // the reader's lock is taken already
        async::shared_mutex::shared_guard second = std::move(first);
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

- [scoped_lock_shared](../shared_mutex/scoped_lock_shared.md): locks and makes the guard
- [release](release.md): gives the lock up without giving it back
- [sgcl::async::shared_mutex::shared_guard](../shared_mutex-shared_guard.md)
