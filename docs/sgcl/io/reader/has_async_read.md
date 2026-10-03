[sgcl](../../README.md) › [io](../README.md) › [reader](README.md)

# sgcl::io::reader::has_async_read

```cpp
bool has_async_read() const noexcept;
```

Checks whether [async_read](read.md) is the stream's own: the stream has an `async_read` method, or is a callable
that returns its task. When it is not, the stream has only `read`, and `async_read` runs it on the
[blocking pool](../../async/spawn_blocking.md), a thread of which it holds for the whole wait. A program that reads from
tasks asks this where a stream given to it would hold a thread of the pool.

## Parameters

None.

## Return value

`true` when the stream has a read for a task of its own; `false` when it has only `read`, and for an empty reader.

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
    println("{} {}", both.has_async_read(), async_only.has_async_read());
    println("{}", blocking.has_async_read());
}
```

Output:

```text
true true
false
```

## See also

- [has_read](has_read.md): whether `read` is the stream's own
- [read, async_read](read.md)
- [sgcl::io::reader](README.md)
