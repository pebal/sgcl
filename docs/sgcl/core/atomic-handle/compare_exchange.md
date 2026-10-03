[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](../atomic-handle.md)

# sgcl::atomic\<H\>::compare_exchange_weak, compare_exchange_strong

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

The compare-exchange of `std::atomic` on the handle's word: when the atomic holds the object `expected` holds, it is
replaced by the object of `desired` and `true` is returned; otherwise `expected` is set to a handle to the object
there now, loaded with `acquire`, and `false` is returned.

The comparison is of identity, the object, as a compare-exchange on a word compares: two strings of the same
characters made apart are two objects, and the expected one must be the one loaded or stored from here, not one
equal to it. A change of contents loads, decides, and exchanges against what it loaded.

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

int main() {
    atomic<string> log = string("start");

    vector<thread> writers;
    for (int t : range(3)) {
        writers.emplace_back([&log, t] {
            string seen = log.load();
            string next;
            do {
                next = seen + string(",") + string(1, char('a' + t));  // the contents decided
            } while (!log.compare_exchange_weak(seen, next));  // against what was loaded
        });
    }
    for (auto& w : writers) {
        w.join();
    }
    println("{}", log.load().size());

    string equal("start,a,b,c");  // equal characters perhaps, never the same object
    println("{}", log.compare_exchange_strong(equal, string("reset")));
}
```

Output:

```text
11
false
```

## See also

- [exchange](exchange.md): replaces the handle unconditionally
- [load, operator H](load.md): the read a failure makes
- [sgcl::atomic\<H\>](../atomic-handle.md)
