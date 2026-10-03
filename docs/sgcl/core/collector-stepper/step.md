[sgcl](../../README.md) › [core](../README.md) › [collector](../collector/README.md) › [stepper](README.md)

# sgcl::collector::stepper::step

```cpp
phase step() noexcept;
```

Lets the collector through one gate: it runs the phase of the cycle up to the next gate, and the call returns when
it stands there. From `released` the next gate is the `start` of the next cycle.

## Parameters

None.

## Return value

The [phase](../collector-stepper-phase.md) the collector stands at then.

## Complexity

The time of one phase of a cycle.

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
    println("{}", s.step() == collector::stepper::phase::start);
    println("{}", s.step() == collector::stepper::phase::flipped);
}
```

Output:

```text
true
true
```

## See also

- [advance_to](advance_to.md): the gates up to one of a name
- [current](current.md): the gate the collector stands at
- [sgcl::collector::stepper](README.md)
