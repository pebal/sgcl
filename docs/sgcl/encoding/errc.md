[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::errc

```cpp
#include "sgcl/encoding/error.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    enum class errc : uint8_t;
}
```

`sgcl::encoding::errc` is what went wrong in the input of a format of the module, the [code](error/code.md) of an
[error](error/README.md). It is one list for every format, so that a program handling the errors of JSON, CSV and base64
learns one set of names and does it with one `switch`; a format uses the codes that mean something for it and no
other. The list was one from the start, before the formats that raise its later codes were written.

It is also an error-code enumeration of the category `"encoding"` ([encoding_category](encoding_category.md)): when
a decoder is read as an [io::reader](../io/reader/README.md), its read fails with an [io::error](../io/error/README.md) whose code
is the `errc`, and the decoder's `last_error()` holds the whole [error](error/README.md) with its offset.

The values start at 1: an `error_code` of 0 is success, and a code of this list travels as an `error_code`.
`std::is_error_code_enum<errc>` is true, so an `errc` converts to an `error_code` by itself
([make_error_code](make_error_code.md)), and `code == encoding::errc::invalid_character` compares an `error_code`
with it directly. The words of a code, in quotes below, are what an error's [message()](error/message.md) says when
the format gave no words of its own.

| Value | Description |
|---|---|
| `syntax` | "syntax error": a character the grammar does not allow here. base64, base32 and hex: data after the padding, padding where the data cannot end, bits past the data in the last character; ascii85: a `z` inside a group; PEM: a boundary line, a label, a header or an `END` that does not match; CSV: a bare quote, a character after a closing quote; JSON, XML |
| `unexpected_end` | "unexpected end of input": the input ends in the middle of a value. The byte codecs and varint: a cut input; PEM: no block, no `END` line; CSV: a quoted field not closed; JSON, XML |
| `invalid_character` | "invalid character": a byte outside the alphabet of base64, base32, hex or ascii85; a character XML does not allow |
| `invalid_utf8` | "invalid UTF-8": JSON, XML |
| `invalid_escape` | "invalid escape sequence": `\x` in JSON, `&#xD800;` in XML |
| `depth_limit` | "nesting too deep": JSON and XML past their depth |
| `duplicate_key` | "duplicate key": a key given twice in a JSON object, an attribute given twice in an XML tag |
| `out_of_range` | "value out of range": a varint past 64 bits, an ascii85 group past 32; `1e400`, `300` read into an `uint8_t` (JSON, CSV and XML fields of a type); a CSV record or an XML token of a stream past its bound |
| `type_mismatch` | "type mismatch": `"abc"` where a field of a type is a number (JSON, CSV, XML) |
| `missing_field` | "missing field": a field marked `required()` is absent (JSON, CSV, XML) |
| `unknown_field` | "unknown field": a key no field has, with JSON's `options.reject_unknown_fields` |
| `unsupported_value` | "unsupported value": NaN written into JSON, a cycle in a graph, a field of a kind CSV has no text for |
| `field_count` | "wrong number of fields": CSV, a record with more or fewer fields than the first |
| `mismatched_tag` | "mismatched tag": XML, `</b>` after `<a>` |
| `undefined_entity` | "undefined entity": XML, `&nbsp;` with no DTD to define it |
| `unsupported_encoding` | "unsupported encoding": XML, `encoding="Shift_JIS"` |
| `io` | "input/output error": the source or the sink failed, a file did not open; the error's [io_error()](error/io_error.md) says how |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer in("QUJ*");
    auto decoder = encoding::base64::standard.decoder_from(in);
    array<byte, 16> out;
    auto read = decoder.read(out);
    error_code code = read.error().code();
    println("{} {}", code.category().name(), code == encoding::errc::invalid_character);
    println("{}", read.error().message());
    println("{}", decoder.last_error()->message());

    error_code depth = encoding::errc::depth_limit;
    println("{} {}", depth.value(), depth.message());
}
```

Output:

```text
encoding true
decode base64: invalid character
offset 3: invalid character '*'
6 nesting too deep
```

## See also

- [error](error/README.md): the code with its place
- [make_error_code](make_error_code.md), [encoding_category](encoding_category.md): an `errc` as an `error_code`
- [io::error](../io/error/README.md): a stream's error, its code an `errc` when a decoder failed
- [sgcl::encoding](README.md)
