# sgcl::encoding::error, sgcl::encoding::errc

```cpp
#include "sgcl/encoding/error.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    enum class errc : uint8_t;   // what went wrong, one list for every format
    class error;                 // the code, where, and the stream's error
    const std::error_category& encoding_category() noexcept;
    error_code make_error_code(errc e) noexcept;
}
```

The error of every format of the module, one type under each format's name: `encoding::base64::error`, `encoding::hex::error`, `encoding::pem::error`, `encoding::varint::error`, and later `json::error`, `csv::error`, `xml::error`. One type to learn, and the name in the code says where the error came from.

## Rules

- A value: copied, compared, held in an `expected`. Nothing in the module throws on its input; a function that reads a whole input returns `expected<T, error>`.
- The offset is the byte of the input where the input stops being the start of something valid: the character outside the alphabet, the padding where the data cannot end, the first character after the padding, and the end of the input when it is cut. A strict base64 or base32 refuses bits past the data at the character that carries them.
- The line and the column are counted after the fact, from the offset and the text, and only when something failed: `locate(text)`. A decoding that succeeds never counts a line ending. A format without lines (base64 of one string, varint) leaves them 0, and the message says the offset instead.
- The column counts code points, not bytes: it is read by a person looking at the line. The byte is `offset()`.
- `errc` is an error-code enumeration of the category `"encoding"`: when a decoder is read as an [`io::reader`](../io/stream.md), its read fails with an `io::error` whose code is the `errc`, and `last_error()` of the decoder holds the `error` with its offset.

## Members

```cpp
enum class errc : uint8_t {
    syntax = 1,          // a character the grammar does not allow here
    unexpected_end,      // the input ends in the middle of a value
    invalid_character,   // a byte outside the alphabet
    invalid_utf8,
    invalid_escape,
    depth_limit,
    duplicate_key,
    out_of_range,        // a varint past 64 bits, an ascii85 group past 32
    type_mismatch,
    missing_field,
    unknown_field,
    unsupported_value,
    field_count,
    mismatched_tag,
    undefined_entity,
    unsupported_encoding,
    io                   // the source or the sink failed: io_error()
};
```

The codes from `invalid_utf8` to `unsupported_encoding` belong to JSON, CSV and XML, the next stages of the module; the list is one from the start, so that a program handling the errors of several formats does it with one `switch`.

```cpp
class error {
public:
    error() = default;
    error(errc code, uint64_t offset, const string& detail = {});   // detail: what message() says instead of the code's words
    error(const io::error& e, uint64_t offset);                             // code io

    errc code() const noexcept;
    uint64_t offset() const noexcept;                   // bytes from the start of the input
    uint32_t line() const noexcept;                     // from 1; 0 when not known
    uint32_t column() const noexcept;                   // from 1, in code points
    const string& path() const noexcept;                // "/users/3/age" (JSON Pointer), "" when it does not apply
    const optional<io::error>& io_error() const noexcept;
    string message() const;                             // "offset 17: invalid character '*'", "3:14 /users/3/age: ..."

    error& locate(const string& text) noexcept;              // the line and the column of offset() in the text
    error& set_position(uint32_t line, uint32_t column) noexcept;
    error& set_path(const string& path);
};
```

`message()` is the position (`line:column` when known, `offset N` otherwise), the path when there is one, and what went wrong — the detail the format gave, or the words of the code; the error of the stream follows when there was one. The setters are for the formats built on this type and for a program that reads an input of its own and wants its errors in the same shape.

## Example

```cpp
using namespace sgcl;

auto r = encoding::base64::standard.decode("QUJ*");
if (!r) {
    auto& e = r.error();
    e.code();        // errc::invalid_character
    e.offset();      // 3
    e.message();     // "offset 3: invalid character '*'"
}

auto p = encoding::pem::parse("x\n-----BEGIN A-----\nQUJD\n-----END B-----\n");
p.error().line();     // 4
p.error().column();   // 10
p.error().message();  // "4:10: END B does not match BEGIN A"

error mine(errc::syntax, 5, "unexpected ','");
mine.locate("[1,\n2,,3]").message();   // "2:2: unexpected ','"
```

## See also

[`io::error`](../io/error.md), which carries an `errc` when a decoder fails as a stream; [the module](README.md).
