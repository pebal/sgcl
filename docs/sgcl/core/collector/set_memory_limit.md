[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::set_memory_limit

```cpp
static void set_memory_limit(size_t bytes) noexcept;
```

Sets the ceiling on committed managed memory to `bytes` ([The memory limit](../collector.md#the-memory-limit)).
Above 75% of it the collector cycles every 100 ms and returns every free chunk at once; when an allocation would
cross it, the allocation forces a full collection and waits for it, and if that does not free enough the program
prints one line to stderr, `sgcl: out of managed memory: N bytes committed, limit L`, and ends with
`std::terminate()`. `0` disables the ceiling; an allocation the system then refuses ends the program the same way,
the line saying `no limit`.

The call wins over `SGCL_MEMORY_LIMIT` in the environment. The setting takes effect for the next chunk committed; it
does not shrink what is committed already.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the ceiling in bytes; `0` for none |

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
    size_t limit = collector::get_memory_limit();  // the default ceiling
    collector::set_memory_limit(size_t(4) << 30);  // 4 GB: past it, a full collection, then the end
    println("{} MB", collector::get_memory_limit() >> 20);

    vector<int> numbers(size_t(1) << 20);  // 4 MB of int: under the ceiling
    println("{} ints", numbers.size());

    collector::set_memory_limit(0);
    println("{}", collector::get_memory_limit());
    collector::set_memory_limit(limit);  // back to the default
}
```

Output:

```text
4096 MB
1048576 ints
0
```

## See also

- [set_memory_limit_percent](set_memory_limit_percent.md): the ceiling as a share of the memory
- [get_memory_limit](get_memory_limit.md): the ceiling
- [sgcl::collector](../collector.md)
