[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [writer](README.md)

# sgcl::io::mixin::writer\<Derived\>::write, async_write

```cpp
template<class D>
expected<size_t, error> write(const D& text)                                 // (1)
    noexcept(noexcept(io::write(std::declval<Derived&>(), text)));
expected<size_t, error> write(byte b)                                        // (2)
    noexcept(noexcept(io::write(std::declval<Derived&>(), b)));
template<class D>
async::task<expected<size_t, error>> async_write(const D& text) noexcept;    // (3)
async::task<expected<size_t, error>> async_write(byte b) noexcept;           // (4)
```

Writes text or one byte to this stream, through `Derived`'s `write` of bytes: [io::write](../../write.md) over this
stream.

- (1, 3) `text` is a [string](../../../core/string/README.md), a text slice (`slice<const char>`, a line of a
  [buffered_reader](../../buffered_reader/README.md), a piece of a string), a literal or a character array up to its first
  NUL and never past its end, a C string (`const char*` or `char*`, to its NUL), or a `std::string_view`, written from
  where it lies with no string made. They take part only for text; bytes are written by `Derived`'s own
  `write(slice<const byte>)`, beside these.
- (2, 4) Writes the one byte `b`.
- (1–2) Write on the calling thread.
- (3–4) The same for a task, with `Derived`'s `async_write`; instantiated only where they are called, over a
  `Derived` that has it. The stream is held by reference while the task runs, and the text from where it lies: a
  string or a slice keeps its owner in the frame, a literal is static, the byte (4) is kept in the frame; a
  `std::string_view`, a character array and a C string are the caller's to keep until the task is done.

A class that defines `write` hides these overloads and brings them back by
`using mixin::writer<Derived>::write;` and `using mixin::writer<Derived>::async_write;`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to write |
| `b` | the byte to write |

## Return value

The number of bytes written, all of them, or the error of the write, as the stream gave it.

## Complexity

Linear in the size of the text: one write of the stream.

## Exceptions

- (1–2) What `Derived`'s `write` throws; none when it is noexcept.
- (3–4) None. What the write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("first line\nsecond line\n"));
    auto line = in.read_line();

    io::buffer out;
    out.write("got: ");
    out.write(**line);  // a slice of the reader's block, no string made
    out.write(byte('\n'));
    out.async_write(string("and from a task\n")).wait();
    print("{}", out.text());
}
```

Output:

```text
got: first line
and from a task
```

## See also

- [io::write](../../write.md): the same over any stream, bytes too
- [copy_from](copy_from.md): everything a reader gives
- [sgcl::io::mixin::writer\<Derived\>](README.md)
