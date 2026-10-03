[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](README.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::wait

```cpp
void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept;      // (1)
void wait(tracked_ptr<T> p, std::memory_order m = std::memory_order_seq_cst) const noexcept;    // (2)
```

The waiting of `std::atomic`: blocks while the word equals the pointer given, until a
[notify_one](notify_one.md) or [notify_all](notify_all.md) after a store wakes the thread and it finds the word
changed. As with `std::atomic`, a wake may also be spurious; the call returns only when the word differs.

1. Blocks while the word is null.
2. Blocks while the word is `p`.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer waited away from |
| `m` | the memory order of the reads, as for `std::atomic::wait` |

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

int main() {
    atomic<tracked_ptr<int>> slot;
    // the lambda captures a reference: no tracked_ptr copied to the heap
    thread producer([&slot] {
        slot.store(make_tracked<int>(1));
        slot.notify_one();
    });
    slot.wait(nullptr);  // until the slot is not null
    println("{}", *slot.load());
    producer.join();
}
```

Output:

```text
1
```

## See also

- [notify_one](notify_one.md), [notify_all](notify_all.md): wake the waiting threads
- [sgcl::atomic\<tracked_ptr\<T\>\>](README.md)
