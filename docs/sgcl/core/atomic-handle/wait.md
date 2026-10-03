[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](README.md)

# sgcl::atomic\<H\>::wait

```cpp
void wait(const H& h, std::memory_order m = std::memory_order_seq_cst) const noexcept;
```

The waiting of `std::atomic` on the handle's word: blocks while the atomic holds the object `h` holds, until a
[notify_one](notify_one.md) or [notify_all](notify_all.md) after a store wakes the thread and it finds another
object there. The comparison is of identity, as for the compare-exchanges; a wake may also be spurious, and the call
returns only when the object differs.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle waited away from |
| `m` | the memory order of the reads, as for `std::atomic::wait` |

## Return value

None.

## Complexity

The time until the object changes.

## Exceptions

None.

## Notes

`wait` returns nothing: the new handle is read with [load](load.md) after it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    atomic<string> state = string("idle");
    string idle = state.load();
    thread worker([&state] {
        state = string("done");
        state.notify_one();
    });
    state.wait(idle);  // until another string is there
    println("{}", state.load());
    worker.join();
}
```

Output:

```text
done
```

## See also

- [notify_one](notify_one.md), [notify_all](notify_all.md): wake the waiting threads
- [sgcl::atomic\<H\>](README.md)
