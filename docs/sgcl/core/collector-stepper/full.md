[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md) › [stepper](../collector-stepper.md)

# sgcl::collector::stepper::full

```cpp
void full(bool full) noexcept;
```

Sets the kind of the stepped cycles from the next one on: full, or young. A young cycle keeps the marks of the
previous cycles and traces only the objects marked for the first time and the old objects on pages a pointer was
stored into ([config](../config.md): `generational`).

## Parameters

| Parameter | Description |
|---|---|
| `full` | `true` for full cycles, `false` for young ones |

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
    auto before = collector::get_statistics();

    s.full(false);
    s.finish_cycle();
    s.finish_cycle();
    auto after = collector::get_statistics();
    println("{} cycles, {} full", after.cycles - before.cycles,
            after.full_cycles - before.full_cycles);
}
```

Output:

```text
2 cycles, 1 full
```

## See also

- [(constructor)](collector-stepper.md): the kind given at the start
- [sgcl::collector::stepper](../collector-stepper.md)
