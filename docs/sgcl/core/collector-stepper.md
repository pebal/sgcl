[sgcl](../README.md) › [core](README.md) › [collector](collector.md)

# sgcl::collector::stepper

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        class stepper;
    };
}
```

`sgcl::collector::stepper` is the collector one gate at a time, for the tests of the engine. While a `stepper`
exists no cycle runs by itself: the collector stands at a gate, a boundary between the phases of a cycle, until
[step()](collector-stepper/step.md) lets it through to the next, and the calling thread, the mutator of the test,
does its work in between, in the window a race would otherwise have to land in by luck. The gates are the values of
[phase](collector-stepper-phase.md): `start`, `flipped`, `registered`, `roots`, `marked`, `swept`, `released`.

The cycles are full unless the stepper is made with `full = false` or [full(false)](collector-stepper/full.md) is
called; sequences in the tests that settle the heap run two full cycles to clear what the previous test left. A
cycle in flight when the stepper is made runs to its end unstepped; the destructor lets the collector run on by
itself.

What the tests of the engine assert with it (`tests/core/stepping.cpp`): an object made after the flip and released
into an old object is neither swept this cycle nor lost by the next; one made before the flip and released after
the roots were traced is reachable by the state of its release alone; a store into an old, marked object after its
page was traced is found by the next young cycle through the card; a `weak_ptr` locked before the weak phase holds
its object through the cycle and reads null after it; a pointer loaded from an `atomic` after the stacks were
scanned, its only other reference dropped, is a root by the state of the copy; a thread exiting before the scan
takes its objects with it, one exiting after keeps them for the cycle; an object watched by an `expiry_queue` is
kept for the drain; a block of cells is freed by the cycle after the one that saw its last cell go.

## Rules

- The stepper's thread is the mutator, and it must not wait for the collector between gates: `force_collect`,
  `get_live_object_count`, `get_live_objects`, `get_type_statistics` and the other queries that wait for a cycle
  would deadlock. `get_statistics()` reads the counters of the last completed cycle and is fine.
- A stepper is neither copyable nor assignable.

## Member types

| Type | Definition |
|---|---|
| [phase](collector-stepper-phase.md) | the gates, the boundaries between the phases of a cycle |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](collector-stepper/collector-stepper.md) | takes the collector over at its next gate |
| `(destructor)` | lets the collector run on by itself |
| [step](collector-stepper/step.md) | lets the collector through one gate |
| [advance_to](collector-stepper/advance_to.md) | lets the collector through the gates up to the next one of a name |
| [finish_cycle](collector-stepper/finish_cycle.md) | lets the collector finish the current cycle |
| [full](collector-stepper/full.md) | sets the kind of the cycles from the next one on |
| [helpers](collector-stepper/helpers.md) | sets the number of helper threads for every pass |
| [current](collector-stepper/current.md) | the gate the collector stands at |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    tracked_ptr<Node> next;
};

int main() {
    collector::stepper s(false);  // young cycles
    tracked_ptr holder = make_tracked<Node>();
    s.finish_cycle();  // holder is old and marked
    s.advance_to(collector::stepper::phase::roots);  // the stacks scanned, the dirty pages traced
    holder->next = make_tracked<Node>();  // stored into an old object after the trace: the card
    s.finish_cycle();  // not swept: made after the flip
    s.finish_cycle();  // registered now, found through the card
    weak_ptr<Node> next = holder->next;
    println("{}", next.expired());
}
```

Output:

```text
false
```

## See also

- [Methods useful for state analysis](../../garbage_collector/diagnostics.md#methods-useful-for-state-analysis): the
  collector's diagnostics
- `tests/core/stepping.cpp`: the races of the engine, stepped
- [sgcl::collector](collector.md)
