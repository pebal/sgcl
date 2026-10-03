[sgcl](../../README.md) › [io](../README.md) › [discard_writer](README.md)

# sgcl::io::discard_writer::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& data) noexcept;                // (1)
async::task<expected<size_t, error>> async_write(slice<const byte> data) noexcept;    // (2)
```

Takes `data` and drops it: nothing is copied and nothing is kept.

1. Returns at once.
2. Returns a task that completes at its first step, with no wait.

The class brings in the overloads of [mixin::writer](../mixin/writer/write.md) as well, which take a text or one
byte.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to drop |

## Return value

`data.size()`: the write never fails.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> quiet() {
    auto n = co_await io::discard.async_write("into the void");
    println("{} bytes dropped in a task", *n);
}

int main() {
    vector<byte> block(4096);
    println("{} bytes dropped", *io::discard.write(block));
    async::run(quiet());
}
```

Output:

```text
4096 bytes dropped
13 bytes dropped in a task
```

## See also

- [sgcl::io::discard_writer](README.md)
