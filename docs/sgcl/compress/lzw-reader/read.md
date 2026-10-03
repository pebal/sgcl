[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md) › [reader](../lzw-reader.md)

# sgcl::compress::lzw::reader::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& out);                         // (1)
async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept;    // (2)
```

Decompresses into `out`: as many bytes as the decoder has ready, at least one unless the stream ended, reading `in`
when it needs more input. 0 is the end of the data (the end code). Data the format does not allow is the error of the
read that reaches it, after every byte before it was handed out, and of every read after.

1. Blocks the calling thread for the reads of `in`.
2. Returns a task that does the same, gives the worker back while `in` reads, and yields every 64 KB handed out. The
   bytes go into `out` when the task runs: the buffer lives until the task is done. A task's read of `in` may run on the
   [blocking pool](../../async/spawn_blocking.md), so the reader's task reads go into managed memory.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the decompressed bytes go into |

## Return value

The number of bytes put into `out`, 0 at the end of the stream (or for an empty `out`), or an error: an `io::error`
of the [compress category](../compress_category.md) for the data (the whole [error](../error.md) is in
[last_error](last_error.md)), or the error of `in` as it came.

## Complexity

Linear in the bytes handed out.

## Exceptions

- (1) What the `read` of `in` throws; the errors of the stream and of the data are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::lzw::compress("hello, hello, hello", compress::lzw::order::lsb, 8);
    compress::lzw::reader r{io::buffer(packed), compress::lzw::order::lsb, 8};
    byte chunk[8];
    while (auto n = r.read(chunk)) {
        if (*n == 0) {
            break;
        }
        println("{} bytes: {}", *n, string(slice<const byte>(chunk).first(*n)));
    }
}
```

Output:

```text
8 bytes: hello, h
8 bytes: ello, he
3 bytes: llo
```

In a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> count(io::reader in) {
    compress::lzw::reader r(in, compress::lzw::order::lsb, 8);
    byte chunk[4096];
    size_t total = 0;
    while (auto n = co_await r.async_read(chunk)) {
        if (*n == 0) {
            break;
        }
        total += *n;
    }
    co_return total;
}

int main() {
    string text = string("the same words, the same words again and again. ").repeat(1000);
    auto packed = compress::lzw::compress(text, compress::lzw::order::lsb, 8);
    println("{}", async::spawn(count(io::buffer(packed))).wait());
}
```

Output:

```text
48000
```

## See also

- [last_error](last_error.md): the whole error of the data
- [sgcl::compress::lzw::reader](../lzw-reader.md)
