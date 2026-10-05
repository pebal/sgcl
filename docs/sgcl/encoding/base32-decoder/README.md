[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32/README.md)

# sgcl::encoding::base32::decoder

```cpp
#include "sgcl/encoding/base32.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class base32 {
    public:
        class decoder;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::base32::decoder` is a reader of the bytes another reader's text decodes to: Go's
`base32.NewDecoder`, made by a codec's [decoder_from](../base32/decoder_from.md) with the codec's alphabet, padding
and strictness. The text may come in pieces of any size; a [read](read.md) into a buffer smaller
than a group gets the group through the decoder. It is an `io::req::reader`, and its async form, so it goes
wherever io takes a reader: `io::copy(file, plain)`, a `buffered_reader` over it.

A text the codec refuses fails the read that reaches it, after the bytes before the error were handed out, and
every read after. The read's `io::error` has the code in the `encoding` category, and
[last_error](last_error.md) holds the [error](../error/README.md) with its offset in the text.

## Rules

- A decoder is a handle: one word, a tracked word to the stream's state, made by the codec
  (`encoding::base32::decoder plain = encoding::base32::standard.decoder_from(r);`) and shared by its copies
  ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds
  none (`!plain`), and an operation on it is a contract violation, checked by `assert`.
- A stream made of one (`io::reader r = plain;`, `io::copy`) binds the state, so the handle may go first.
- The state is a managed object that holds its block, 8 KB of `array<byte, N>` as io's buffers are, and the
  reader under it. The text is read into the block and decoded straight into the caller's buffer.
- One thread or task at a time, as on any stream; a read waits as the reader under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](base32-decoder.md) | a decoder that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector |
| `operator=` | the handle of another decoder: both share its stream |

#### Input

| Function | Description |
|---|---|
| [read, async_read](read.md) | decoded bytes into a buffer |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | where the text went wrong, or why the reader under it failed |
| [operator bool](operator_bool.md) | whether the handle holds a stream |

#### From io::mixin::reader

The reads of every reader of the library ([io::mixin::reader](../../io/mixin/reader/README.md)), through the decoder's own read.

| Function | Description |
|---|---|
| `read_full`, `async_read_full` | a buffer filled whole |
| `read_all`, `async_read_all` | the bytes to the end |
| `read_all_text`, `async_read_all_text` | the bytes to the end as a string |
| `copy_to`, `async_copy_to` | the bytes to the end into a writer |

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
    io::buffer text;
    text.write("NBSWY3DPFQQHO33SNRSA====");
    encoding::base32::decoder plain = encoding::base32::standard.decoder_from(text);
    array<byte, 5> chunk;
    for (;;) {
        auto n = plain.read(chunk);
        if (!n || *n == 0) {
            break;
        }
        println("'{}'", string(chunk.as_slice(0, *n)));
    }
}
```

Output:

```text
'hello'
', wor'
'ld'
```

## See also

- [decoder_from](../base32/decoder_from.md): the decoder of a codec
- [base32::encoder](../base32-encoder/README.md): the other way
- [io streams](../../io/README.md)
- [sgcl::encoding::base32](../base32/README.md)
