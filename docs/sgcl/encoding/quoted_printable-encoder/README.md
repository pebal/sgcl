[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md)

# sgcl::encoding::quoted_printable::encoder

```cpp
#include "sgcl/encoding/quoted_printable.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class quoted_printable {
    public:
        class encoder;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::quoted_printable::encoder` is a writer whose bytes go out encoded to another writer, made by
a codec's [encoder_to](../quoted_printable/encoder_to.md) in the codec's form: Go's `quotedprintable.Writer`. A
[write](write.md) encodes its bytes and writes the text to the writer under it; a space or a tab waits for the
byte after it; [close](close.md) writes what waits and leaves the writer under it open, as a MIME part's
boundary follows the text. It is an `io::req::writer` and an `io::req::closer`, and their async forms.

## Rules

- A encoder is a handle: one word, a tracked word to the stream's state, made by the codec and shared by its
  copies ([operator==](operator_cmp.md) says whether two are the same). A default-constructed one holds none
  (`!h`), and an operation on it is a contract violation, checked by `assert`.
- A stream made of one (`io::writer r = h;`, `io::copy`) binds the state, so the handle may go first.
- A failure of the writer under it is kept for good: every later `write` and `close` reports it. A write after `close()` is `io::errc::closed`; nothing is written when the encoder dies without `close()`.
- One thread or task at a time, as on any stream; a write waits as the stream under it does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](quoted_printable-encoder.md) | a encoder that holds none, or a copy that shares the stream |
| `(destructor)` | drops the handle; the state is left to the collector |
| `operator=` | the handle of another encoder: both share its stream |

#### Output

| Function | Description |
|---|---|
| [write, async_write](write.md) | encodes bytes into the writer under it |
| [close, async_close](close.md) | writes what waits; the writer under it stays open |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | whether the encoder was closed |
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
    io::buffer out;
    auto enc = encoding::quoted_printable::standard.encoder_to(out);
    enc.write("price: 5€ ");
    enc.close();  // the space at the end is escaped
    println("{}", out.text());
}
```

Output:

```text
price: 5=E2=82=AC=20
```

## See also

- [encoder_to](../quoted_printable/encoder_to.md): what makes one
- [quoted_printable](../quoted_printable/README.md)
