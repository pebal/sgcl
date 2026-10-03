[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](README.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::compare_exchange_weak, compare_exchange_strong

```cpp
bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t,                                   // (1)
                           const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n,                                 // (2)
                           const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t,                                   // (3)
                           const std::memory_order s, const std::memory_order f) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n,                                 // (4)
                           const std::memory_order s, const std::memory_order f) noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t,                                 // (5)
                             const std::memory_order m = std::memory_order_seq_cst)
    noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n,                               // (6)
                             const std::memory_order m = std::memory_order_seq_cst)
    noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t,                                 // (7)
                             const std::memory_order s, const std::memory_order f) noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n,                               // (8)
                             const std::memory_order s, const std::memory_order f) noexcept;
```

The compare-exchange of `std::atomic`: when the word equals `e`, it is replaced by `n` (or null) and `true` is
returned; otherwise `e` is set to the current value, loaded with `acquire` and held, and `false` is returned. A
successful exchange carries the write barrier.

- (1–4) The `weak` form may fail spuriously, when the word equals `e`, and belongs in a loop.
- (5–8) The `strong` form fails only when the word is not `e`.
- (1–2), (5–6) With one order `m`, the failure order is derived from it as `std::atomic` does.
- (3–4), (7–8) With two, `s` is the order of the success and `f` of the failure.

No ABA: the object `e` holds cannot be reused while `e` holds it, so an equal address is the same object.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the pointer expected; on a failure, the current one |
| `n` | the pointer stored on a success |
| `m` | the memory order of the operation |
| `s`, `f` | the memory orders of the success and of the failure |

## Return value

`true` when the pointer was replaced, `false` otherwise.

## Complexity

Constant; on a failure, a [load](load.md) as well.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value;
    tracked_ptr<Item> next;
};

int main() {
    atomic<tracked_ptr<Item>> head;

    // push: the new item's next is the head as last seen, until the exchange lands
    for (int i : range(3)) {
        tracked_ptr item = make_tracked<Item>(i);
        item->next = head.load();
        while (!head.compare_exchange_weak(item->next, item)) {
        }
    }

    // pop: the head replaced by its next, or null
    tracked_ptr top = head.load();
    while (top && !head.compare_exchange_weak(top, top->next)) {
    }
    println("{} {}", top->value, head.load()->value);

    tracked_ptr<Item> expected;  // null: fails, and loads the current head into expected
    println("{} {}", head.compare_exchange_strong(expected, nullptr), expected->value);
}
```

Output:

```text
2 1
false 1
```

## See also

- [exchange](exchange.md): replaces the pointer unconditionally
- [load, operator tracked_ptr\<T\>](load.md): the read a failure makes
- [sgcl::atomic\<tracked_ptr\<T\>\>](README.md)
