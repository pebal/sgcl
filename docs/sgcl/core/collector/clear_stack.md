[sgcl](../../README.md) › [core](../README.md) › [collector](README.md)

# sgcl::collector::clear_stack

```cpp
static void clear_stack(size_t bytes = config::stack_clear_size) noexcept;
```

Zeroes `bytes` of the unused stack below the caller's frame (`SIZE_MAX` for the whole unused stack), never closer
than `config::stack_guard_margin` to the end of the thread's stack and never pages the stack has not touched.
Objects referenced only by words left behind in dead frames become collectable.

[force_collect](force_collect.md) and the counting functions call it before they count; on its own it is for a
long-lived loop that wants a stale root gone before the next cycle rather than before the next count.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | how much of the unused stack to zero: 64 KB by default ([config](../config.md): `stack_clear_size`), `SIZE_MAX` for all of it |

## Return value

None.

## Complexity

Linear in `bytes`, up to the pages the stack has touched.

## Exceptions

None.

## Notes

The stack is scanned conservatively, so a word a dead frame left behind keeps its object until it is overwritten.
The call cannot clear the caller's own frame: a raw pointer or an iterator kept there does retain its target. It is
declared always-inline, so that no frame of its own lies between the caller and the area it zeroes.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Batch {
    int items[64];
};

// deep frames, now dead, may hold stale pointers
static int process_batch() {
    tracked_ptr batch = make_tracked<Batch>();
    return int(sizeof(batch->items));
}

int main() {
    int total = 0;
    for (int i : range(3)) {
        total += process_batch();
        collector::clear_stack();  // 64 KB below this frame zeroed
    }
    collector::clear_stack(SIZE_MAX);  // or the whole unused stack
    println("{} bytes processed", total);
}
```

Output:

```text
768 bytes processed
```

## See also

- [force_collect](force_collect.md): a full collection, after the same zeroing
- [config](../config.md): `stack_clear_size`, `stack_guard_margin`
- [Stack roots](../../../garbage_collector/overview.md#stack-roots): how the stacks are scanned
- [sgcl::collector](README.md)
