[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [writer](../csv-writer.md)

# sgcl::encoding::csv::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;    // (1)
writer(const io::writer& out, const options& o);    // (2)
writer(const writer&) = delete;                     // (3)
```

Constructs a writer into the stream `out`: a file, a socket, an encoder, an [io::buffer](../../io/buffer.md),
`io::stdout`, any [io::writer](../../io/writer.md). The writer never closes it.

1. With a comma between the fields.
2. With the separator and the comment character of `o`, checked here as a reader checks them; the other options
   are a reader's.
3. A writer is not copied, assigned or moved.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the stream the text is written into |
| `o` | the options, of which the writer reads `separator` and `comment` |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `invalid_argument` when the separator is a quote, `'\r'`, `'\n'`, NUL or a byte past ASCII, or the comment
  character is the separator, a quote, a line ending or a byte past ASCII.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::writer w(io::stdout, {.separator = '\t'});
    w.write({"a", "b c", "d\te"});
    w.flush().value();
}
```

Output:

```text
a	b c	"d	e"
```

## See also

- [options](../csv-options.md): the separator
- [sgcl::encoding::csv::writer](../csv-writer.md)
