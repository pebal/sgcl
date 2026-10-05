[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md)

# sgcl::encoding::quoted_printable::decoder

```cpp
#include "sgcl/encoding/quoted_printable.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class quoted_printable {
    public:
        class decoder;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::quoted_printable::decoder` is a reader of the bytes the text of another reader decodes to,
made by a codec's [decoder_from](../quoted_printable/decoder_from.md) with the codec's decoding: Go's
`quotedprintable.Reader`. It is an `io::req::reader` and its async form, so it goes wherever io takes a
reader: `io::read_all(dec)`, `io::copy(file, dec)`.

## Rules

- A decoder is a handle: one word, a tracked word to the stream's state, made by the codec and shared by its
  copies ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds none
  (`!h`), and an operation on it is a contract violation, checked by `assert`.
- A stream made of one (`io::reader r = h;`, `io::copy`) binds the state, so the handle may go first.
- A text a strict decoding refuses fails the read with an `io::error` of the encoding category, and [last_error](last_error.md) keeps the [error](../error/README.md) with its offset.
- One thread or task at a time, as on any stream; a read waits as the stream under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](quoted_printable-decoder.md) | a decoder that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector |
| `operator=` | the handle of another decoder: both share its stream |

#### Input

| Function | Description |
|---|---|
| [read, async_read](read.md) | the next decoded bytes |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | where the text went wrong, or why the reader under it failed |
| [operator bool](operator_bool.md) | whether the handle holds a stream |

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
    io::buffer in;
    in.write("line one=20\r\nline=\r\n two\r\n");
    auto dec = encoding::quoted_printable::standard.decoder_from(in);
    print("{}", io::read_all_text(dec).value());
}
```

Output:

```text
line one 
line two
```

## See also

- [decoder_from](../quoted_printable/decoder_from.md): what makes one
- [quoted_printable](../quoted_printable/README.md)
