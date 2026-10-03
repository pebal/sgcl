[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85/README.md)

# sgcl::encoding::ascii85::encoder

```cpp
#include "sgcl/encoding/ascii85.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ascii85 {
    public:
        class encoder;
    };
}
```

`sgcl::encoding::ascii85::encoder` is a writer whose bytes go out encoded to another writer: Go's
`ascii85.NewEncoder`, made by [ascii85::encoder_to](../ascii85/encoder_to.md). A [write](write.md)
encodes the whole groups of four bytes it has and writes them to the writer under it; the bytes short of a group
wait for the next write; [close](close.md) writes the last group, n + 1 characters for n bytes. It
is an `io::req::writer` and an `io::req::closer`, and their async forms, so it goes wherever io takes a writer:
`io::copy(a85, file)`, an `io::writer` field.

`close()` leaves the writer under it open, as Go's does, since what is written around the text usually goes
on: the `~>` of Adobe's variant, the rest of a PostScript or PDF file.

## Rules

- An encoder is a handle: one word, a tracked word to the stream's state, made by `ascii85::encoder_to`
  (`encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(w);`) and shared by its copies
  ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds
  none (`!a85`), and an operation on it is a contract violation, checked by `assert`.
- It lies on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted](../../core/rooted/README.md) of it. A stream made of one (`io::writer w = a85;`, `io::copy`) binds the
  state, so the handle may go first.
- The state is a managed object that holds its block, 8 KB of `array<byte, N>` as io's buffers are, and the
  writer under it. Nothing is written when it dies: an encoder dropped without `close()` loses its last group.
- A failure of the writer under it is kept for good, as Go's encoder keeps it: the group being written went with
  it, so every later `write` and `close` reports that failure rather than going on without those bytes. A write
  after `close()` is `io::errc::closed`.
- One thread or task at a time, as on any stream; a write waits as the writer under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ascii85-encoder.md) | an encoder that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector, and nothing is written |
| `operator=` | the handle of another encoder: both share its stream |

#### Output

| Function | Description |
|---|---|
| [write, async_write](write.md) | encodes bytes into the writer under it |
| [close, async_close](close.md) | writes the last group; the writer under it stays open |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | whether the encoder was closed |
| [operator bool](operator_bool.md) | whether the handle holds a stream |

#### From io::mixin::writer

The text and the copies of every writer of the library ([io::mixin::writer](../../io/mixin/writer/README.md)), through the
encoder's own write.

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
    io::buffer out;
    out.write("<~");
    encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(out);
    a85.write("Hello, ");
    a85.write("World!");
    a85.close();  // the last group, short
    out.write("~>");
    println(out.text());
}
```

Output:

```text
<~87cURD_*#4DfTZ)+T~>
```

## See also

- [encoder_to](../ascii85/encoder_to.md): the encoder
- [ascii85::decoder](../ascii85-decoder/README.md): the other way
- [io streams](../../io/README.md)
- [sgcl::encoding::ascii85](../ascii85/README.md)
