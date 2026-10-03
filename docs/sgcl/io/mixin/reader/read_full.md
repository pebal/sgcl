[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [reader](../reader.md)

# sgcl::io::mixin::reader\<Derived\>::read_full, async_read_full

```cpp
expected<size_t, error> read_full(const slice<byte>& buffer)                                // (1)
    noexcept(noexcept(io::read_full(std::declval<Derived&>(), buffer)));
async::task<expected<size_t, error>> async_read_full(const slice<byte>& buffer) noexcept    // (2)
    requires req::async_reader<Derived&>;
```

Fills the whole of `buffer` from this stream, reading as many times as it takes; the stream ending part way is
`errc::unexpected_eof`, ending before the first byte is the end of the stream, 0. It is
[io::read_full](../../read_full.md) over this stream, where the reads and the errors are described.

1. Reads on the calling thread.
2. The same for a task, with `Derived`'s `async_read`; takes part only when `Derived` has it. The stream is held by
   reference while the task runs: the caller keeps it alive until the task is done, which a `co_await` in the same
   statement does by itself.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | the bytes to fill, all of them |

## Return value

`buffer.size()`; 0 when the stream ends before the first byte, or when `buffer` is empty. Or the error:
`errc::unexpected_eof` (`is_eof()`) when the stream ends part way, the bytes read until then at the front of
`buffer` and their number the error's [count](../../error/count.md); the error of a read, as the stream gave it.

## Complexity

Linear in `buffer.size()`; the number of reads is the stream's.

## Exceptions

- (1) What `Derived`'s `read` throws; none when it is noexcept.
- (2) None. What a read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer packet("GIF89a, then a short rest");
    vector<byte> magic(6);
    packet.read_full(magic);
    println("{}", string(magic));

    vector<byte> block(100);
    auto rest = packet.async_read_full(block).wait();
    println("{}: {} bytes", rest.error().message(), rest.error().count());
    println("{}", packet.read_full(block).value());  // nothing left: the end
}
```

Output:

```text
GIF89a
read: unexpected end of stream: 19 bytes
0
```

## See also

- [io::read_full](../../read_full.md): the same over any stream
- [read_all](read_all.md): everything to the end of the stream
- [sgcl::io::mixin::reader\<Derived\>](../reader.md)
