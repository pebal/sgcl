[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::exchange

```cpp
H exchange(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the handle viewed and returns the old one: the handle viewed holds the object `h` holds, and the handle
returned the object that was there, held from before the exchange on. The order is at least `acq_rel` whatever `m`
asks for.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle stored |
| `m` | the memory order, as for `std::atomic_ref::exchange` |

## Return value

A handle to the object that was there.

## Complexity

Constant: a compare-exchange, repeated while other threads change the word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string current = "v1";
    string previous = atomic_ref(current).exchange(string("v2"));
    println("{} -> {}", previous, current);
}
```

Output:

```text
v1 -> v2
```

## See also

- [compare_exchange_weak, compare_exchange_strong](compare_exchange.md): replaces the handle when it holds the
  object expected
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
