[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::set_memory_limit_percent

```cpp
static void set_memory_limit_percent(unsigned percent) noexcept;
```

Sets the ceiling on committed managed memory ([The memory limit](../collector.md#the-memory-limit)) to `percent`
of the memory the process may use: the cgroup limit on Linux, else the physical memory. The ceiling is `0`,
disabled, when the system does not say how much that is. The call wins over `SGCL_MEMORY_LIMIT` in the
environment, and takes effect as [set_memory_limit](set_memory_limit.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `percent` | the share, from 1 to 100; debug builds assert the range |

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
    size_t limit = collector::get_memory_limit();
    collector::set_memory_limit_percent(100);
    size_t all = collector::get_memory_limit();
    collector::set_memory_limit_percent(50);
    println("{}", collector::get_memory_limit() * 2 == all);
    collector::set_memory_limit(limit);
}
```

Output:

```text
true
```

## See also

- [set_memory_limit](set_memory_limit.md): the ceiling in bytes
- [config](../config.md): `heap_limit_percent`, the default share
- [sgcl::collector](../collector.md)
