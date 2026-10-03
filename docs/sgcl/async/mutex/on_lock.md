[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::on_lock

```cpp
template<class F>
auto on_lock(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

Makes a case of a [select](../select.md) that locks the mutex: when the select takes the case, the mutex is locked
and `f()` is called. The case is the channel's receive of the mutex's signal, served when the signal is there, so a
select may wait for the mutex beside a channel, a timer or a stop.

The mutex stays locked when `f` returns: the case takes the lock and hands it to the program, which unlocks it with
[unlock](unlock.md), in `f` or after the select.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments once the mutex is locked |

## Return value

A case of a [select](../select.md), of a type of the library. The select waits until one of its cases can be
served; when it takes this one, the mutex is locked and `f()` runs. With [otherwise](../otherwise.md) beside it, a
mutex that is locked is not waited for.

## Complexity

Constant to make. Served, a receive through the channel's ring.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what `f` throws comes
out of the select, and the mutex is locked.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::mutex m;
    for (int i : range(2)) {
        async::select(
            m.on_lock([] { println("locked"); }),
            async::otherwise([] { println("busy"); })
        ).wait();
    }
    m.unlock();  // the lock the first case took
}
```

Output:

```text
locked
busy
```

## See also

- [select](../select.md): the cases and how one is chosen
- [try_lock](try_lock.md): the lock without a wait, outside a select
- [scoped_lock](scoped_lock.md): the lock of a task
- [sgcl::async::mutex](../mutex.md)
