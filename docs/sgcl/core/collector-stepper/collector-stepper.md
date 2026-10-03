[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md) › [stepper](../collector-stepper.md)

# sgcl::collector::stepper::stepper

```cpp
/*(1)*/ explicit stepper(bool full = true) noexcept;
/*(2)*/ stepper(const stepper&) = delete;
```

1. Takes the collector over: from here on no cycle runs by itself, and the collector stands at a gate until
   [step](step.md) lets it through. A cycle in flight runs to its end unstepped first. The cycles are full, or
   young with `full = false`; [full](full.md) changes it later.
2. A stepper is not copyable.

The destructor, `~stepper() noexcept`, gives the collector back: it runs on by itself from the gate it stands at.

## Parameters

| Parameter | Description |
|---|---|
| `full` | whether the stepped cycles are full or young |

## Complexity

The rest of a cycle in flight.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    {
        collector::stepper s;
        s.finish_cycle();
        size_t cycles = collector::get_statistics().cycles;
        collector::force_collect();  // a request: no cycle runs while the stepper holds it
        this_thread::sleep_for(std::chrono::milliseconds(50));
        println("{}", collector::get_statistics().cycles == cycles);
    }  // the collector runs on by itself
    println("{}", collector::force_collect(true));
}
```

Output:

```text
true
true
```

## See also

- [step](step.md): one gate
- [full](full.md): the kind of the cycles
- [sgcl::collector::stepper](../collector-stepper.md)
