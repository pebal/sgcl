[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](README.md)

# sgcl::atomic_ref\<H\>::compare_exchange_weak, compare_exchange_strong

```cpp
bool compare_exchange_weak(H& expected, const H& desired,                                      // (1)
                           const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_weak(H& expected, const H& desired, const std::memory_order s,           // (2)
                           const std::memory_order f) noexcept;
bool compare_exchange_strong(H& expected, const H& desired,                                    // (3)
                             const std::memory_order m = std::memory_order_seq_cst)
    noexcept;
bool compare_exchange_strong(H& expected, const H& desired, const std::memory_order s,         // (4)
                             const std::memory_order f) noexcept;
```

The compare-exchange of `std::atomic_ref` on the handle's word: when the handle viewed holds the object `expected`
holds, it is replaced by the object of `desired` and `true` is returned; otherwise `expected` is set to a handle to
the object there now, loaded with `acquire`, and `false` is returned.

The comparison is of identity, the object, as for [atomic\<H\>](../atomic-handle/compare_exchange.md): the expected
handle must be one loaded or stored from here, not one equal to it.

- (1–2) The `weak` form may fail spuriously and belongs in a loop.
- (3–4) The `strong` form fails only when the object is not the one expected.
- (1), (3) With one order `m`, the failure order is derived from it as `std::atomic` does.
- (2), (4) With two, `s` is the order of the success and `f` of the failure.

## Parameters

| Parameter | Description |
|---|---|
| `expected` | the handle expected; on a failure, the handle there now |
| `desired` | the handle stored on a success |
| `m` | the memory order of the operation |
| `s`, `f` | the memory orders of the success and of the failure |

## Return value

`true` when the handle was replaced, `false` otherwise.

## Complexity

Constant; on a failure, a [load](load.md) as well.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Document {
    string title = "draft";
};

int main() {
    tracked_ptr doc = make_tracked<Document>();
    atomic_ref title(doc->title);

    string seen = title.load();
    string retitled = seen + string(" 2");
    println("{}", title.compare_exchange_strong(seen, retitled));

    string stale("draft");  // the old contents, not the object there
    println("{} {}", title.compare_exchange_strong(stale, string("final")), stale);
}
```

Output:

```text
true
false draft 2
```

## See also

- [exchange](exchange.md): replaces the handle unconditionally
- [load, operator H](load.md): the read a failure makes
- [sgcl::atomic_ref\<H\>](README.md)
