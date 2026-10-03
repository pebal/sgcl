[sgcl](../../README.md) › [core](../README.md) › [collector](../collector/README.md) › [stepper](README.md)

# sgcl::collector::stepper::current

```cpp
phase current() const noexcept;
```

The gate the collector stands at, without moving it.

## Parameters

None.

## Return value

The [phase](../collector-stepper-phase.md) of the gate.

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
    println("{}", s.current() == collector::stepper::phase::released);
    s.step();
    println("{}", s.current() == collector::stepper::phase::start);
}
```

Output:

```text
true
true
```

## See also

- [step](step.md): one gate
- [sgcl::collector::stepper](README.md)
