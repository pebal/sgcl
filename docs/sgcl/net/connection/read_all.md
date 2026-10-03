[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::read_all, async_read_all

```cpp
/*(1)*/ expected<vector<byte>, io::error> read_all() const;
/*(2)*/ async::task<expected<vector<byte>, io::error>> async_read_all() const noexcept;
```

Reads everything to the end of the stream, until the peer closes its writing half: Go's `io.ReadAll(c)`. It is
[io::read_all](../../io/read_all.md) over the connection.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits.

## Parameters

None.

## Return value

The bytes, empty when the stream was at its end; or the [io::error](../../io/error.md) of the [read](read.md) that
failed, the bytes read before it lost.

## Complexity

Linear in the bytes read.

## Exceptions

- (1) `std::system_error` when a read has to wait and the thread of the reactor, or of the timers, which its first
  use starts, cannot be made.
- (2) None.

## Notes

The input is the network's and nothing bounds it: a peer that never stops writing fills the memory. A deadline
([set_read_deadline](set_read_deadline.md)) bounds the time; a [limit_reader](../../io/limit_reader.md) over the
connection bounds the bytes.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto [a, b] = net::connection::in_memory();
    auto writing = async::spawn([a]() -> async::task<> {
        co_await a.async_write("one ");
        co_await a.async_write("two");
        a.close_write();
    });
    vector<byte> all = b.read_all().value();
    println("{} bytes", all.size());
    writing.wait();
}
```

Output:

```text
7 bytes
```

## See also

- [read_all_text](read_all_text.md): the same into a `string`
- [read_full](read_full.md): a buffer of a known size
- [sgcl::net::connection](../connection.md)
