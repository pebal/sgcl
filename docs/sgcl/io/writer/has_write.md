[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::has_write

```cpp
bool has_write() const noexcept;
```

Checks whether [write](write.md) is the stream's own: the stream has a `write` method, or is a callable of its
shape. When it is not, the stream has only `async_write`, and `write` waits for its task on the calling thread.

## Parameters

None.

## Return value

`true` when the stream has a blocking write of its own; `false` when it has only `async_write`, and for an empty
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
    println("{} {} {}", both.has_write(), async_only.has_write(), blocking.has_write());
}
```

Output:

```text
true false true
```

## See also

- [has_async_write](has_async_write.md): whether `async_write` is the stream's own
- [write, async_write](write.md)
- [sgcl::io::writer](../writer.md)
