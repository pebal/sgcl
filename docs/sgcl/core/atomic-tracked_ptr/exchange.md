[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](../atomic-tracked_ptr.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::exchange

```cpp
/*(1)*/ tracked_ptr<T> exchange(tracked_ptr<T> n,
                                const std::memory_order m = std::memory_order_seq_cst) noexcept;
/*(2)*/ tracked_ptr<T> exchange(std::nullptr_t,
                                const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer and returns the old one, held: the old object is under the hazard pointer from before the
exchange until the returned `tracked_ptr` holds it, so it cannot be reclaimed in between. The exchange carries the
write barrier.

1. With `n`.
2. With null.

The order is at least `acq_rel` whatever `m` asks for: the exchange after the hazard's store must not pass it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the pointer stored |
| `m` | the memory order, as for `std::atomic::exchange` |

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

struct Node {
    int value;
    tracked_ptr<Node> next;
};

int main() {
    atomic<tracked_ptr<Node>> head;
    for (int i : range(3)) {
        head = make_tracked<Node>(i, head.load());
    }

    tracked_ptr all = head.exchange(nullptr);  // the whole list taken, in one step
    for (tracked_ptr n = all; n; n = n->next) {
        print("{} ", n->value);
    }
    println("{}", head.load() == nullptr);
}
```

Output:

```text
2 1 0 true
```

## See also

- [compare_exchange_weak, compare_exchange_strong](compare_exchange.md): replaces the pointer when it is the one
  expected
- [sgcl::atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr.md)
