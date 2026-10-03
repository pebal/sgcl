[sgcl](../../README.md) › [core](../README.md) › [collector](../collector/README.md) › [stepper](README.md)

# sgcl::collector::stepper::finish_cycle

```cpp
void finish_cycle() noexcept;
```

Lets the collector finish the current cycle: the gates up to `released`, `advance_to(phase::released)`. Standing
at `released`, it runs the whole next cycle.

## Parameters

None.

## Return value

None.

## Complexity

The rest of the cycle.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value = 0;
};

// the pointer in a frame of its own: the stack is scanned conservatively
static weak_ptr<Item> make_and_drop() {
    tracked_ptr item = make_tracked<Item>();
    return item;
}

int main() {
    collector::stepper s;
    weak_ptr<Item> weak = make_and_drop();
    collector::clear_stack();
    s.finish_cycle();
    s.finish_cycle();
    println("{}", weak.expired());
}
```

Output:

```text
true
```

## See also

- [advance_to](advance_to.md): the gates up to one of a name
- [sgcl::collector::stepper](README.md)
