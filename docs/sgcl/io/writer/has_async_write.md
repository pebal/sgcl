[sgcl](../../README.md) › [io](../README.md) › [writer](README.md)

# sgcl::io::writer::has_async_write

```cpp
bool has_async_write() const noexcept;
```

Checks whether [async_write](write.md) is the stream's own: the stream has an `async_write` method, or is a
callable that returns its task. When it is not, the stream has only `write`, and `async_write` runs it on the
[blocking pool](../../async/spawn_blocking.md), a thread of which it holds for the whole write.

## Parameters

None.

## Return value

`true` when the stream has a write for a task of its own; `false` when it has only `write`, and for an empty
writer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Uplink {  // async_write alone
    async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
        co_return data.size();
    }
};

int main() {
    Uplink uplink;
    io::writer both = io::buffer();
    io::writer async_only = uplink;
    io::writer blocking = [](slice<const byte>) {};
    println("{} {}", both.has_async_write(), async_only.has_async_write());
    println("{}", blocking.has_async_write());
}
```

Output:

```text
true true
false
```

## See also

- [has_write](has_write.md): whether `write` is the stream's own
- [write, async_write](write.md)
- [sgcl::io::writer](README.md)
