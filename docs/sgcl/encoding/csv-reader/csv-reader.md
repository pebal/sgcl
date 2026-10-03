[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [reader](../csv-reader.md)

# sgcl::encoding::csv::reader::reader

```cpp
explicit reader(const string& text) noexcept;      // (1)
reader(const string& text, const options& o);      // (2)
explicit reader(const io::reader& in) noexcept;    // (3)
reader(const io::reader& in, const options& o);    // (4)
reader(const reader&) = delete;                    // (5)
```

Constructs a reader at the start of its input. Nothing is read until the first record is asked for.

- (1–2) A reader of a text in memory, read where it is; the reader keeps the text alive.
- (3–4) A reader of a stream: a file, a socket, a decoder, an [io::buffer](../../io/buffer.md), any
  [io::reader](../../io/reader.md). It reads the stream a block at a time when a record is asked for, and never
  closes it.
- (1), (3) With the default [options](../csv-options.md): a comma, no comments, every record as long as the
  first.
- (2), (4) With the options given, checked here.
- (5) A reader is not copied, assigned or moved: it holds the record being read.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the CSV text |
| `in` | the stream the text is read from |
| `o` | the separator, the comment character, Go's other settings and the bound of a record |

## Complexity

Constant.

## Exceptions

- (1), (3) None.
- (2), (4) `invalid_argument` when the separator is a quote, `'\r'`, `'\n'`, NUL or a byte past ASCII, or the
  comment character is the separator, a quote, a line ending or a byte past ASCII.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader text("a,b\n");
    println("{}", text.next()->at(1));

    io::buffer in("x\ty\n");
    encoding::csv::reader stream(in, {.separator = '\t'});
    println("{}", stream.next()->at(1));

    try {
        encoding::csv::reader wrong("a\n", {.separator = '\n'});
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
b
y
sgcl::encoding::csv: the separator is a quote, a line ending, NUL or not ASCII
```

## See also

- [options](../csv-options.md): what the reader is given
- [next](next.md): the first record
- [sgcl::encoding::csv::reader](../csv-reader.md)
