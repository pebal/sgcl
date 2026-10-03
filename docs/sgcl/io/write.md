[sgcl](../README.md) › [io](README.md)

# sgcl::io::write, async_write

```cpp
#include "sgcl/io/functions.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<req::writer W, class D>
    expected<size_t, error> write(W&& w, const D& data) noexcept(/* see below */);    // (1)
    template<req::writer W>
    expected<size_t, error> write(W&& w, byte b) noexcept(/* see below */);           // (2)
    template<req::async_writer W, class D>
    async::task<expected<size_t, error>> async_write(W&& w, const D& data)            // (3)
        noexcept(/* see below */);
    template<req::async_writer W>
    async::task<expected<size_t, error>> async_write(W&& w, byte b)                   // (4)
        noexcept(/* see below */);
}
```

Writes data to the stream `w`, the type of the data telling what it is, each from where it lies, with no string
made: Go's `io.WriteString` and `Write` in one name.

- (1, 3) `data` is bytes, whatever converts to `slice<const byte>` (a slice, a `vector<byte>`, an `array<byte, N>`),
  or text: a [string](../core/string/README.md), a text slice (`slice<const char>`, a line of a
  [buffered_reader](buffered_reader/README.md), a piece of a string), a literal or a character array up to its first NUL and
  never past its end (an array filled to the brim has none), a C string (`const char*` or `char*`, to its NUL; a null
  pointer does not compile), a `std::string_view`. They take part only for such data.
- (2, 4) Writes the one byte `b`.

- (1–2) Write on the calling thread, with the stream's `write`.
- (3–4) The same for a task, with the stream's `async_write`. The data is written from where it lies: a string, a
  slice or a vector keeps its owner in the task's frame, a literal is static, the byte (4) is kept in the frame; a
  `std::string_view`, a character array and a C string are the caller's to keep until the task is done. A stream
  given as a temporary is moved into the frame; one given by reference is the caller's to keep alive.

`w` is any [writer](req/writer.md) (3–4: async writer). A class that carries
[mixin::writer](mixin/writer/README.md) has the text and the byte forms as members, [w.write(text)](mixin/writer/write.md).

## Parameters

| Parameter | Description |
|---|---|
| `w` | the stream to write to |
| `data` | the bytes or the text to write |
| `b` | the byte to write |

## Return value

The number of bytes written, all of them, or the error of the write of `w`, as it gave it.

## Complexity

Linear in the size of the data: one write of the stream.

## Exceptions

- (1–2) What the write of `w` throws, and (1) what the conversion of `data` to `slice<const byte>` throws; none when
  both are noexcept, and then the function is declared noexcept.
- (3–4) What the move of a stream given as a temporary into the task's frame throws, and (3) what the conversion of
  `data` throws; none for a stream given by reference. What the write throws is the task's: its `co_await`
  rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    string name = "streams";
    const char* c_string = "a C string";
    std::string_view view = "a view";
    vector<byte> bytes = {byte('o'), byte('k')};

    io::write(io::stdout, "hello, ");
    io::write(io::stdout, name);
    io::write(io::stdout, byte('\n'));
    io::write(io::stdout, c_string);
    io::write(io::stdout, ", ");
    io::write(io::stdout, view);
    io::write(io::stdout, ", ");
    io::write(io::stdout, bytes);
    io::write(io::stdout, byte('\n'));

    io::buffer out;
    auto n = io::async_write(out, name.as_slice(0, 6)).wait();
    println("{} bytes: {}", *n, out.text());
}
```

Output:

```text
hello, streams
a C string, a view, ok
6 bytes: stream
```

## See also

- [mixin::writer::write](mixin/writer/write.md): the same as a member of a stream
- [print](print.md), [println](println.md): formatted text on a stream
- [copy](copy.md): a whole reader into a writer
- [req::writer, async_writer](req/writer.md): what `w` may be
