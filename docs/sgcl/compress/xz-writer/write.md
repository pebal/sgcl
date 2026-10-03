[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz.md) › [writer](../xz-writer.md)

# sgcl::compress::xz::writer::write, async_write

```cpp
/*(1)*/ expected<size_t, io::error> write(const slice<const byte>& data);
/*(2)*/ async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept;
```

Takes `data` into the writer's window and codes what the parser can see far enough ahead of, writing to `out` what the
coder makes. The first write takes the window and the tables and writes the header of the stream and of the block;
options out of range fail it with `errc::invalid_argument`.

1. Blocks the calling thread for the work and the writes of `out`.
2. Returns a task that does the same in portions of 64 KB with a yield between them, and gives the worker back
   while `out` writes. The bytes are read when the task runs: `data` lives until the task is done.

The text forms (a string, a literal, a `std::string_view`) and one byte come from
[mixin::writer](../../io/mixin/writer.md).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |

## Return value

The number of bytes taken, `data.size()`, or the writer's error: a failure of `out` (kept as the first error),
options out of range, a write after the close (`io::errc::closed`, kept too), or the first error the writer gave before.

## Complexity

Linear in the size of `data`, by a factor the level sets.

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
    compress::xz::writer w(sink);
    auto n = w.write("hello, hello, hello");
    println("{} bytes taken, {} written", *n, sink.size());
    (void)w.close();
    println("{} written after the close", sink.size());
}
```

Output:

```text
19 bytes taken, 24 written
72 written after the close
```

In a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> pack(io::buffer sink) {
    compress::xz::writer w(sink, {.level = 0});
    for (int i : {1, 2, 3}) {
        co_await w.async_write(string("the same words again and again "));
    }
    co_await w.async_close();
}

int main() {
    io::buffer sink;
    async::spawn(pack(sink)).wait();
    compress::xz::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
93
```

## See also

- [close](close.md): the rest of the output
- [sgcl::compress::xz::writer](../xz-writer.md)
