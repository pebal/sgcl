[sgcl](../../README.md) › [io](../README.md) › [reader](README.md)

# sgcl::io::reader::has_read

```cpp
bool has_read() const noexcept;
```

Checks whether [read](read.md) is the stream's own: the stream has a `read` method, or is a callable of its shape.
When it is not, the stream has only `async_read`, and `read` waits for its task on the calling thread.

## Parameters

None.

## Return value

`true` when the stream has a blocking read of its own; `false` when it has only `async_read`, and for an empty
reader.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Ticker {  // async_read alone
    async::task<expected<size_t, io::error>> async_read(slice<byte>) {
        co_return 0;
    }
};

int main() {
    Ticker ticker;
    io::reader both = io::buffer("both");
    io::reader async_only = ticker;
    io::reader blocking = [](slice<byte>) -> size_t { return 0; };
    println("{} {} {}", both.has_read(), async_only.has_read(), blocking.has_read());
}
```

Output:

```text
true false true
```

## See also

- [has_async_read](has_async_read.md): whether `async_read` is the stream's own
- [read, async_read](read.md)
- [sgcl::io::reader](README.md)
