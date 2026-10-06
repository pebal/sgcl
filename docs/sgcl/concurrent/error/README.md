[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::error

```cpp
#include "sgcl/concurrent/error.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    class error;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

Why bytes are not a sketch: what the `from_bytes` of a [bloom_filter](../bloom_filter/README.md), a
[hyperloglog](../hyperloglog/README.md) and a [count_min_sketch](../count_min_sketch/README.md) give in their
`expected` when the bytes do not read. A sentence, [message](message.md), and the byte of the input the reading stopped
on, [offset](offset.md): the shape of [time::error](../../time/error/README.md), one type of error for the whole module.
A value, not an exception, as every failure of the library is an [expected](../../core/expected/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error.md) | constructs an error of a sentence and a byte |
| [message](message.md) | the sentence |
| [offset](offset.md) | the byte of the input |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compares two errors |

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> truncated = concurrent::hyperloglog(4).to_bytes();
    truncated.pop_back();
    auto r = concurrent::hyperloglog::from_bytes(truncated);
    println("{} at byte {}", r.error().message(), r.error().offset());
}
```

Output:

```text
a HyperLogLog's size does not match its precision at byte 23
```

## See also

- [time::error](../../time/error/README.md): the same shape in the time module
