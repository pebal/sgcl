[sgcl](../../README.md) › [core](../README.md) › [collector](../collector/README.md) › [stepper](README.md)

# sgcl::collector::stepper::helpers

```cpp
void helpers(unsigned n) noexcept;
```

Forces `n` helper threads on every pass of the cycles from here on, however small the heap: the parallel marking
with its work stealing, the sweep, the stack scan and the states pass, which the policy starts only from a million
objects or 256 pages ([config](../config.md): `mark_object_threshold`, `sweep_page_threshold`). `0` is the policy
again. It is set while the collector stands at a gate; set at `released`, the next cycle runs with them, and at its
`released` the [statistics](../collector-statistics.md) count them. The library's own scenarios run twice, alone
and with two helpers.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of helper threads; `0` for the policy |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    collector::stepper s;
    s.finish_cycle();
    s.helpers(2);
    s.finish_cycle();
    auto stats = collector::get_statistics();
    println("{} helpers started, {} used", stats.helper_threads, stats.last_helpers_used);
    s.helpers(0);
    s.finish_cycle();
}
```

Output:

```text
2 helpers started, 2 used
```

## See also

- [full](full.md): the kind of the cycles
- [statistics](../collector-statistics.md): `last_helpers_used`
- [sgcl::collector::stepper](README.md)
