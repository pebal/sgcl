[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::wait

```cpp
/*(1)*/ void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept;
/*(2)*/ void wait(tracked_ptr<T> p, std::memory_order m = std::memory_order_seq_cst) const noexcept;
```

The waiting of `std::atomic_ref`: blocks while the word viewed equals the pointer given, until a
[notify_one](notify_one.md) or [notify_all](notify_all.md) after a store wakes the thread and it finds the word
changed. A wake may also be spurious; the call returns only when the word differs.

1. Blocks while the word is null.
2. Blocks while the word is `p`.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer waited away from |
| `m` | the memory order of the reads, as for `std::atomic_ref::wait` |

## Return value

None.

## Complexity

The time until the word changes.

## Exceptions

None.

## Notes

`wait` returns nothing: the new pointer is read with [load](load.md) after it, which holds the object.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Slot {
    tracked_ptr<int> value;
};

int main() {
    tracked_ptr slot = make_tracked<Slot>();
    thread producer([slot] {  // the pointer goes with the closure, into a managed node
        atomic_ref(slot->value).store(make_tracked<int>(1));
        atomic_ref(slot->value).notify_one();
    });
    atomic_ref(slot->value).wait(nullptr);  // until the slot is not null
    println("{}", *atomic_ref(slot->value).load());
    producer.join();
}
```

Output:

```text
1
```

## See also

- [notify_one](notify_one.md), [notify_all](notify_all.md): wake the waiting threads
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
