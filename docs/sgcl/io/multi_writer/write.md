[sgcl](../../README.md) › [io](../README.md) › [multi_writer](../multi_writer.md)

# sgcl::io::multi_writer::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& data);                         // (1)
async::task<expected<size_t, error>> async_write(slice<const byte> data) noexcept;    // (2)
```

Writes `data` to every writer, in their order. The first error stops it: it is returned as it is, the writers
before it have the data, and the writers after it are not written to.

1. Writes with each writer's `write`.
2. Returns a task that writes with each writer's `async_write`, one after another; a writer that has only `write`
   has it run on the [blocking pool](../../async/spawn_blocking.md) by its handle.

The class brings in the overloads of [mixin::writer](../mixin/writer/write.md) as well, which write a text or one
byte through these.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |

## Return value

`data.size()`, or the error of the first writer that failed.

## Complexity

That of the writes, one per writer.

## Exceptions

- (1) What a writer's `write` throws.
- (2) None. What a writer's write throws is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer before, after;
    io::writer full = [](slice<const byte>) -> expected<size_t, io::error> {
        error_code code = std::make_error_code(std::errc::no_space_on_device);
        return unexpected(io::error(code, "write", "disk"));
    };
    io::multi_writer out({before, full, after});
    auto r = out.write("data");
    println("{}", r.error().message());
    println("before: [{}], after: [{}]", before.text(), after.text());
}
```

Output:

```text
write disk: No space left on device
before: [data], after: []
```

## See also

- [sgcl::io::multi_writer](../multi_writer.md)
