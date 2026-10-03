[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md) › [writer](../gzip-writer.md)

# sgcl::compress::gzip::writer::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data);                         // (1)
async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept;    // (2)
```

Compresses `data` into `out`, in pieces of 64 KB, writing to `out` what each piece makes. The encoder keeps the last
bytes it has not decided on yet, so what is written comes out behind the input; [flush](flush.md) or [close](close.md)
sends the rest. The first write writes the header before the data, and fails with `errc::invalid_argument` when the
format cannot write it; the error's message says what is wrong with it.

1. Blocks the calling thread for the writes of `out`.
2. Returns a task that does the same and gives the worker back between the pieces (a yield) and while `out`
   writes. The bytes are read when the task runs: `data` lives until the task is done.

The text forms (a string, a literal, a `std::string_view`) and one byte come from
[mixin::writer](../../io/mixin/writer.md).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |

## Return value

The number of bytes taken, `data.size()`, or the writer's error: a failure of `out` (kept as the first error), a
write after the close (`io::errc::closed`), a header the format cannot write, or the first error the writer gave before.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream and of the format are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::gzip::writer w(sink);
    auto n = w.write("hello, hello, hello");
    println("{} bytes taken, {} written", *n, sink.size());
    (void)w.close();
    println("{} written after the close", sink.size());
}
```

Output:

```text
19 bytes taken, 10 written
28 written after the close
```

A header the format cannot write:

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::gzip::writer w(sink, {.header = {.extra = vector<byte>(70000)}});
    println("{}", w.write("croissant, 4.50").error().message());
    println("{} written", sink.size());
}
```

Output:

```text
write gzip: an extra field longer than 65535 bytes: invalid argument
0 written
```

In a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> pack(io::buffer sink) {
    compress::gzip::writer w(sink);
    for (int i : {1, 2, 3}) {
        co_await w.async_write(string("the same words again and again "));
    }
    co_await w.async_close();
}

int main() {
    io::buffer sink;
    async::spawn(pack(sink)).wait();
    compress::gzip::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
93
```

## See also

- [flush](flush.md), [close](close.md): what comes out after the writes
- [sgcl::compress::gzip::writer](../gzip-writer.md)
