[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::wait

```cpp
void wait(const H& h, std::memory_order m = std::memory_order_seq_cst) const noexcept;
```

The waiting of `std::atomic_ref` on the handle's word: blocks while the handle viewed holds the object `h` holds,
until a [notify_one](notify_one.md) or [notify_all](notify_all.md) after a store wakes the thread and it finds
another object there. The comparison is of identity; a wake may also be spurious, and the call returns only when
the object differs.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle waited away from |
| `m` | the memory order of the reads, as for `std::atomic_ref::wait` |

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

struct Task {
    string state = "idle";
};

int main() {
    tracked_ptr task = make_tracked<Task>();
    string idle = atomic_ref(task->state).load();
    thread worker([task] {
        atomic_ref(task->state).store(string("done"));
        atomic_ref(task->state).notify_one();
    });
    atomic_ref(task->state).wait(idle);  // until another string is there
    println("{}", atomic_ref(task->state).load());
    worker.join();
}
```

Output:

```text
done
```

## See also

- [notify_one](notify_one.md), [notify_all](notify_all.md): wake the waiting threads
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
