[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_all, async_read_all

```cpp
#include "sgcl/io/functions.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<req::reader R>
    expected<vector<byte>, error> read_all(R&& r) noexcept(/* see below */);    // (1)
    template<req::async_reader R>
    async::task<expected<vector<byte>, error>> async_read_all(R&& r)            // (2)
        noexcept(/* see below */);
}
```

Reads the stream `r` to its end and returns its bytes, as Go's `io.ReadAll`.

1. Reads on the calling thread, with the stream's `read`. The bytes are gathered in unmanaged memory, grown by
   doubling from io's block size, `config::io_buffer_size` (8 KB), and copied once at the end into a
   `vector<byte>` of exactly their size: one managed allocation, the result. The steps of the growth are scratch
   the collector never sees.
2. The same for a task, with the stream's `async_read`. Its reads go into managed blocks, `config::io_buffer_size`
   first, then blocks of `config::io_copy_buffer_size` (32 KB) for a stream that proves longer, each filled before
   the next is made, and at the end they are copied once into the result and dropped: for 100 KB, 8 + 3 × 32 KB of
   blocks, where a vector grown by doubling would have left 8 + 16 + 32 + 64 + 128. A read of a task may run on the
   blocking pool, and the slice it is given holds its block: a task let go of meanwhile frees nothing the pool writes
   into. A stream given as a temporary is moved into the task's frame; one given by reference is the caller's to keep
   alive until the task is done.

`r` is any [reader](req/reader.md) (2: async reader). A class that carries
[mixin::reader](mixin/reader/README.md) has the same as a member, [r.read_all()](mixin/reader/read_all.md);
[read_all_text](read_all_text.md) gives the bytes as a `string`.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read |

## Return value

The bytes of the stream from its position to its end, an empty vector for a stream at its end, or the error of a
read of `r`, as it gave it; the bytes read before it are dropped.

## Complexity

Linear in the number of bytes read.

## Exceptions

- (1) What the read of `r` throws; none when it is noexcept, as an `io::buffer`'s is, and then `read_all` is
  declared noexcept.
- (2) What the move of a stream given as a temporary into the task's frame throws; none for a stream given by
  reference. What a read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto bytes = io::read_all(io::buffer("hello"));
    println("{} bytes: {}", bytes->size(), string(*bytes));

    io::buffer long_one(string("x").repeat(100000));
    auto more = io::async_read_all(long_one).wait();
    println("{} bytes, {} left", more->size(), long_one.size());
}
```

Output:

```text
5 bytes: hello
100000 bytes, 0 left
```

## See also

- [read_all_text](read_all_text.md): the same as a `string`
- [read_full](read_full.md): a buffer's worth, all of it
- [mixin::reader::read_all](mixin/reader/read_all.md): the same as a member of a stream
- [read_file](read_file.md): a whole file by its path
