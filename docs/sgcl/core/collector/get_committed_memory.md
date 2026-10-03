[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::get_committed_memory

```cpp
static size_t get_committed_memory() noexcept;
```

Bytes of managed memory committed right now: the part of the heap's reserved range backed by physical memory, in
2 MB chunks, the free chunks kept for reuse included ([Memory](../../../garbage_collector/overview.md#memory)). The
reservation itself, the process's virtual size, is not counted.

## Parameters

None.

## Return value

The committed bytes, a multiple of `config::chunk_size`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The committed memory is what the [memory limit](../collector.md#the-memory-limit) bounds. The free chunks kept
committed are `config::heap_free_chunk_reserve` at most ([config](../config.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} MB committed", collector::get_committed_memory() / 1048576);
    vector<int> numbers(size_t(16) << 20);  // 64 MB of int
    println("{} MB committed", collector::get_committed_memory() / 1048576);
}
```

Sample output:

```text
0 MB committed
68 MB committed
```

## See also

- [get_memory_limit](get_memory_limit.md): the ceiling on it
- [get_statistics](get_statistics.md): `committed_bytes` among the other counters
- [sgcl::collector](../collector.md)
