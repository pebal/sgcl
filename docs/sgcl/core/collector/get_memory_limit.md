[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::get_memory_limit

```cpp
static size_t get_memory_limit() noexcept;
```

The ceiling on committed managed memory, in bytes ([The memory limit](../collector.md#the-memory-limit)): by
default 90% of the cgroup memory limit on Linux, or of the physical memory elsewhere (`config::heap_limit_percent`),
or what `SGCL_MEMORY_LIMIT` in the environment says, or what [set_memory_limit](set_memory_limit.md) or
[set_memory_limit_percent](set_memory_limit_percent.md) set last.

## Parameters

None.

## Return value

The ceiling in bytes; `0` when it is disabled.

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
    size_t ceiling = collector::get_memory_limit();
    println("ceiling {} MB, pressure above {} MB", ceiling >> 20,
            (ceiling / 100 * config::heap_pressure_percent) >> 20);
}
```

Sample output:

```text
ceiling 58982 MB, pressure above 44236 MB
```

## See also

- [set_memory_limit](set_memory_limit.md), [set_memory_limit_percent](set_memory_limit_percent.md): set the ceiling
- [get_committed_memory](get_committed_memory.md): what the ceiling bounds
- [sgcl::collector](../collector.md)
