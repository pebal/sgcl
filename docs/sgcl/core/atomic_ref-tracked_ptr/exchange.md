[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::exchange

```cpp
tracked_ptr<T> exchange(tracked_ptr<T> n,                                                   // (1)
                        const std::memory_order m = std::memory_order_seq_cst) noexcept;
tracked_ptr<T> exchange(std::nullptr_t,                                                     // (2)
                        const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer viewed and returns the old one, held: the old object is under the hazard pointer from before
the exchange until the returned `tracked_ptr` holds it, so it cannot be reclaimed in between. The exchange carries
the write barrier.

1. With `n`.
2. With null.

The order is at least `acq_rel` whatever `m` asks for: the exchange after the hazard's store must not pass it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the pointer stored |
| `m` | the memory order, as for `std::atomic_ref::exchange` |

## Return value

The pointer that was there, held.

## Complexity

Constant: a compare-exchange, repeated while other threads change the word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Inbox {
    tracked_ptr<string> message;
};

int main() {
    tracked_ptr inbox = make_tracked<Inbox>();
    atomic_ref(inbox->message).store(make_tracked<string>("hello"));

    tracked_ptr taken = atomic_ref(inbox->message).exchange(nullptr);  // taken, in one step
    println("{} {}", *taken, inbox->message == nullptr);
}
```

Output:

```text
hello true
```

## See also

- [compare_exchange_weak, compare_exchange_strong](compare_exchange.md): replaces the pointer when it is the one
  expected
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
