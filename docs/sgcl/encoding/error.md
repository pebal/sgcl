[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::error

```cpp
#include "sgcl/encoding/error.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class error;
}
```

`sgcl::encoding::error` is why an input is not what its format says: the [code](error/code.md) of what went wrong
(an [errc](errc.md)), the byte of the input it was found at, the line and the column when the format has lines, the
path inside the structure when it has one (JSON, XML), and the error of the stream when a stream failed. It is the
one error of every format of the module, under each format's name — `encoding::base64::error`,
`encoding::base32::error`, `encoding::hex::error`, `encoding::ascii85::error`, `encoding::pem::error`,
`encoding::varint::error`, `encoding::json::error`, `encoding::csv::error`, `encoding::xml::error` are all this
type: one type to learn, and the name in the code says where the error came from. Go has an error type of each
package (`base64.CorruptInputError`, `csv.ParseError`, `json.SyntaxError`); one `switch` over the codes here handles
the errors of several formats.

Nothing in the module throws on its input. A function that reads a whole input returns an
[expected](../core/expected.md)`<T, error>`: the value or the error. A reader of a format a piece at a time (a
[csv::reader](csv-reader.md), a [json::reader](json-reader.md)) stops at the first mistake and keeps the error in
its `last_error()`; a decoder read as an [io::reader](../io/reader.md) fails its read with an
[io::error](../io/error.md) whose code is the error's [errc](errc.md), and keeps the whole error, offset included, in
its `last_error()`.

## Rules

- An error is a value: copied, compared, held in an `expected`. It holds two [strings](../core/string.md) and an
  optional `io::error`, so it lives where a `tracked_ptr` may.
- **The offset is where the input stops being the start of something valid**: the character outside the alphabet,
  the padding where the data cannot end, the first character after the padding, and the end of the input when it is
  cut — `QQ=x` fails at the `x`, `QUJ` at its end. A strict base64 or base32 refuses bits past the data at the
  character that carries them.
- **A position costs nothing until something fails.** The offset is what a decoder has anyway; the line and the
  column are counted from the text after the fact, on the path of the error alone ([locate](error/locate.md)), so
  that a decoding that succeeds never counts a line ending. A format without lines (base64 of one string, varint)
  leaves them 0, and the message says the offset instead.
- **The column counts code points, not bytes**: it is read by a person looking at the line. The byte is
  [offset()](error/offset.md).
- [message()](error/message.md) is the position (`line:column` when known, `offset N` otherwise), the path when
  there is one, and what went wrong — the detail the format gave, or the words of the code; the error of the stream
  follows when there was one: `offset 3: invalid character '*'`, `4:10: END B does not match BEGIN A`.
- **An error that did not come from an input text has no position**: what `stringify`, `from` and `as` of
  [json](json.md) and [xml](xml.md) give, written from a value or read from a tree, a mistake of the calls to an
  [xml::writer](xml-writer.md), and the error of a file of `load` or `save` of json, xml and csv that does not
  open, read or write. Its line, column and offset are 0, and its message is the path and the words:
  `/x: NaN is not a JSON number`, `'two words' is not a qualified name`,
  `input/output error: open cfg.json: No such file or directory`.
- What throws is a mistake in the program, not in its input: an alphabet with a repeated character
  (`invalid_argument`, an error at compile time in a constant), a caller's buffer smaller than the size the codec
  asked for (`length_error`), a PEM type that is not a label, a CSV separator that cannot be one
  (`invalid_argument`).
- The constructors and the setters are for the formats built on this type and for a program that reads an input of
  its own and wants its errors in the same shape.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `base64.CorruptInputError`, `hex.InvalidByteError`, `ascii85.CorruptInputError` | `encoding::base64::error` and the others, this type: the code, the offset and a message |
| `csv.ParseError`, `json.SyntaxError`, `xml.SyntaxError` | the same type: the line and the column, in code points, from [line()](error/line.md) and [column()](error/column.md) |
| `errors.Is(err, csv.ErrFieldCount)` | `e.code() == encoding::errc::field_count` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error/error.md) | an error of a code and an offset, or of a stream's error |

#### Observers

| Function | Description |
|---|---|
| [code](error/code.md) | what went wrong |
| [offset](error/offset.md) | the byte of the input it was found at |
| [line](error/line.md) | the line, from 1; 0 when not known |
| [column](error/column.md) | the column, from 1, in code points |
| [path](error/path.md) | where inside the structure: a JSON Pointer, an XML path |
| [io_error](error/io_error.md) | the stream's error, when a stream failed |
| [message](error/message.md) | the position, the path and the words |

#### Modifiers

| Function | Description |
|---|---|
| [locate](error/locate.md) | the line and the column of the offset in the text |
| [set_position](error/set_position.md) | sets the line and the column |
| [set_path](error/set_path.md) | sets the path |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](error/operator_cmp.md) | the same code, place, words and stream's error |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto decoded = encoding::base64::standard.decode("QUJ*");
    const encoding::error& e = decoded.error();
    println("{} {} {}", e.code() == encoding::errc::invalid_character, e.offset(), e.message());

    auto block = encoding::pem::parse("x\n-----BEGIN A-----\nQUJD\n-----END B-----\n");
    println("{}:{} {}", block.error().line(), block.error().column(), block.error().message());

    encoding::error mine(encoding::errc::syntax, 5, "unexpected ','");
    println("{}", mine.locate("[1,\n2,,3]").message());
}
```

Output:

```text
true 3 offset 3: invalid character '*'
4:10 4:10: END B does not match BEGIN A
2:2: unexpected ','
```

## See also

- [errc](errc.md): the codes and the formats that raise them
- [io::error](../io/error.md): what a decoder read as a stream fails with, its code an `errc`
- [expected](../core/expected.md): the value or the error
- [sgcl::encoding](README.md)
