[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md)

# sgcl::encoding::hex::dumper

```cpp
#include "sgcl/encoding/hex.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class hex {
    public:
        class dumper;
    };
}
```

`sgcl::encoding::hex::dumper` is a writer that writes the dump of what it is given to another writer, the lines
of [hex::dump](../hex/dump.md) and of `hexdump -C`: Go's `hex.Dumper`, made by [hex::dumper_to](../hex/dumper_to.md). A
[write](write.md) writes a line as soon as its sixteen bytes are there, the offsets counted across the
writes; the bytes short of a line wait for the next write, and [close](close.md) writes the short line
at the end. It is an `io::req::writer` and an `io::req::closer`, and their async forms, so it goes wherever io
takes a writer: between a connection and the program, dumping what goes over the wire, or `io::copy(wire, file)`.

`close()` leaves the writer under it open, as the encoders of the module do; Go's `Dumper` closes nothing under
it either.

## Rules

- A dumper is a handle: one word, a tracked word to the stream's state, made by
  `hex::dumper_to` (`encoding::hex::dumper wire = encoding::hex::dumper_to(w);`) and shared by its copies
  ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds
  none (`!wire`), and an operation on it is a contract violation, checked by `assert`.
- It lies on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted](../../core/rooted/README.md) of it. A stream made of one (`io::writer w = wire;`, `io::copy`) binds the
  state, so the handle may go first.
- The state is a managed object that holds its block, 8 KB of `array<byte, N>` as io's buffers are, the line
  being filled and the writer under it. Nothing is written when it dies: a dumper dropped without `close()`
  loses its short line.
- A failure of the writer under it is kept for good: the lines being written went with it, so every later
  `write` and `close` reports that failure rather than going on without them. A write
  after `close()` is `io::errc::closed`.
- One thread or task at a time, as on any stream; a write waits as the writer under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hex-dumper.md) | a dumper that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector, and nothing is written |
| `operator=` | the handle of another dumper: both share its stream |

#### Output

| Function | Description |
|---|---|
| [write, async_write](write.md) | dumps bytes into the writer under it |
| [close, async_close](close.md) | writes the short line; the writer under it stays open |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | whether the dumper was closed |
| [operator bool](operator_bool.md) | whether the handle holds a stream |

#### From io::mixin::writer

The text and the copies of every writer of the library ([io::mixin::writer](../../io/mixin/writer/README.md)), through the
dumper's own write.

| Function | Description |
|---|---|
| `write`, `async_write` | a text (a string, a literal, a `std::string_view`) or a byte |
| `copy_from`, `async_copy_from` | a reader to its end |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two handles share one stream |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer request;
    request.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    encoding::hex::dumper wire = encoding::hex::dumper_to(io::stdout);
    io::copy(wire, request);
    wire.close();
}
```

Output:

```text
00000000  47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a  |GET / HTTP/1.1..|
00000010  48 6f 73 74 3a 20 65 78  61 6d 70 6c 65 2e 63 6f  |Host: example.co|
00000020  6d 0d 0a 0d 0a                                    |m....|
```

## See also

- [dumper_to](../hex/dumper_to.md): the dumper
- [dump](../hex/dump.md): the dump at once
- [io streams](../../io/README.md)
- [sgcl::encoding::hex](../hex/README.md)
