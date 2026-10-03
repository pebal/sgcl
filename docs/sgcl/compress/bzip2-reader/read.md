[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2.md) › [reader](../bzip2-reader.md)

# sgcl::compress::bzip2::reader::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& out);                         // (1)
async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept;    // (2)
```

Decompresses into `out`, decoding straight into it: as many bytes as the decoder can give, at least one unless the
stream ended, reading `in` when it needs more input. 0 is the end of the last stream. Data the format does not
allow is the error of the read that reaches it, after every byte before it was handed out, and of every read after.

1. Blocks the calling thread for the reads of `in`.
2. Returns a task that does the same in portions of 64 KB of output with a yield between them, and gives the worker
   back while `in` reads. The bytes go into `out` when the task runs: the buffer lives until the task is done. A
   task's read of `in` may run on the [blocking pool](../../async/spawn_blocking.md), so the reader's task reads go
   into a managed block of 32 KB.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // "hello, hello, hello" as bzip2 -9 writes it
    auto packed = encoding::hex::decode("425a68393141592653599c453ed3000003910040040244a0002122"
                                        "3030065227e454585dc914e142427114fb4c");
    compress::bzip2::reader r{io::buffer(*packed)};
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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> unpack(io::reader in) {
    compress::bzip2::reader r(in);
    auto text = co_await r.async_read_all_text();
    co_return text.value_or(string());
}

int main() {
    auto packed = encoding::hex::decode("425a68393141592653599c453ed3000003910040040244a0002122"
                                        "3030065227e454585dc914e142427114fb4c");
    println("{}", async::spawn(unpack(io::buffer(*packed))).wait());
}
```

Output:

```text
hello, hello, hello
```

## See also

- [last_error](last_error.md): the whole error of the data
- [sgcl::compress::bzip2::reader](../bzip2-reader.md)
