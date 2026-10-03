[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md)

# sgcl::encoding::hex::encoder

```cpp
#include "sgcl/encoding/hex.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class hex {
    public:
        class encoder;
    };
}
```

`sgcl::encoding::hex::encoder` is a writer whose bytes go out as lower-case hexadecimal digits to another
writer: Go's `hex.NewEncoder`, made by [hex::encoder_to](../hex/encoder_to.md). A byte is a whole group, so a
[write](write.md) writes the digits of every byte it is given to the writer under it, and nothing
waits; [close](close.md) ends the encoder. It is an `io::req::writer` and an `io::req::closer`, and
their async forms, so it goes wherever io takes a writer: `io::copy(digits, file)`, an `io::writer` field.

`close()` leaves the writer under it open, as the encoders of [base64](../base64-encoder/README.md) do; Go's hex encoder
has no `Close`.

## Rules

- An encoder is a handle: one word, a tracked word to the stream's state, made by
  `hex::encoder_to` (`encoding::hex::encoder digits = encoding::hex::encoder_to(w);`) and shared by its copies
  ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds
  none (`!digits`), and an operation on it is a contract violation, checked by `assert`.
- It lies on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted](../../core/rooted/README.md) of it. A stream made of one (`io::writer w = digits;`, `io::copy`) binds the
  state, so the handle may go first.
- The state is a managed object that holds its block, 8 KB of `array<byte, N>` as io's buffers are, and the
  writer under it.
- A failure of the writer under it is kept for good: the digits being written went with it, so every later
  `write` and `close` reports that failure rather than going on without those bytes. A write
  after `close()` is `io::errc::closed`.
- One thread or task at a time, as on any stream; a write waits as the writer under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hex-encoder.md) | an encoder that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector, and nothing is written |
| `operator=` | the handle of another encoder: both share its stream |

#### Output

| Function | Description |
|---|---|
| [write, async_write](write.md) | encodes bytes into the writer under it |
| [close, async_close](close.md) | ends the encoder; the writer under it stays open |

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
    io::buffer packet;
    packet.write("\x01\x02\xFE\xFF");
    io::buffer out;
    encoding::hex::encoder digits = encoding::hex::encoder_to(out);
    println("{} bytes", *io::copy(digits, packet));
    digits.close();
    println(out.text());
}
```

Output:

```text
4 bytes
0102feff
```

## See also

- [encoder_to](../hex/encoder_to.md): the encoder
- [hex::decoder](../hex-decoder/README.md): the other way
- [io streams](../../io/README.md)
- [sgcl::encoding::hex](../hex/README.md)
