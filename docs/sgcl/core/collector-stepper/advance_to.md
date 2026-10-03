[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md) › [stepper](../collector-stepper.md)

# sgcl::collector::stepper::advance_to

```cpp
phase advance_to(phase p) noexcept;
```

Lets the collector through gate after gate until it stands at the next gate named `p`: in this cycle when the
collector stands before it, in the next one otherwise.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the gate to stop at |

## Return value

`p`.

## Complexity

The phases up to the gate.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    collector::stepper s;
    s.advance_to(collector::stepper::phase::roots);  // the stacks scanned
    println("{}", s.current() == collector::stepper::phase::roots);
    s.finish_cycle();
}
```

Output:

```text
true
```

## See also

- [step](step.md): one gate
- [finish_cycle](finish_cycle.md): to the end of the cycle
- [sgcl::collector::stepper](../collector-stepper.md)
