# sgcl::encoding::ascii85

```cpp
#include "sgcl/encoding/ascii85.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class ascii85;            // encode, decode
    class ascii85::encoder;   // a writer and a closer (io::req::writer, io::req::closer)
    class ascii85::decoder;   // a reader (io::req::reader)
}
```

Ascii85 as the `btoa` tool writes it and Go's `encoding/ascii85` reads it: four bytes as a number of 32 bits written in five digits of base 85, the characters `!` (0) to `u` (84) — a quarter more text than the bytes, where base64 takes a third. Four zero bytes are the one character `z`. The last group of 1 to 3 bytes is padded with zeros, encoded, and cut to its first n + 1 characters. Nothing to choose, so every member is static.

## Rules

- No `<~` and `~>` around the text: that is Adobe's variant, a matter of the file that carries it (PostScript, PDF), and the caller's to add or take off.
- The decoding skips every byte up to the space — white space and the control characters — as Go's does, so a text wrapped at any width reads back.
- `z` stands only between groups; inside one it is `syntax` at its offset. A single character after the last group is `unexpected_end`.
- **A group worth more than 32 bits is refused**, `out_of_range` at its fifth character (or at the end, for a short last group padded with `u`). No encoder writes one; Go takes it modulo 2^32 and reads some other bytes. The tests hold both sides to this by name.
- The caller's buffers are as base64's, the sizes bounds rather than counts (`z`), and a buffer too small is `length_error`. The streams are as [base64's](base64.md#rules): `encoder_to(w)` with `close()` writing the last group and leaving `w` open, `decoder_from(r)` taking its text in pieces of any size.

## Members

```cpp
class ascii85 {
public:
    using error = encoding::error;
    class encoder;
    class decoder;

    static string encode(const slice<const byte>& data);
    static string encode(const string& text);                             // the bytes of the text
    static expected<vector<byte>, error> decode(const string& text);
    static size_t max_encoded_size(size_t n) noexcept;                 // the bound: a 'z' makes it fewer
    static constexpr size_t max_decoded_size(size_t n) noexcept;       // 4n: every character a 'z'
    static size_t encode_to(const slice<char>& out, const slice<const byte>& data);                // into the caller's buffer
    static expected<size_t, error> decode_to(const slice<byte>& out, const string& text);
    static tracked_ptr<encoder> encoder_to(const io::writer& out);
    static tracked_ptr<decoder> decoder_from(const io::reader& in);
};
```

`encoder` and `decoder` have the members of [base64's](base64.md#members).

## Example

```cpp
#include "sgcl/encoding/ascii85.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    io::stdout.write(encoding::ascii85::encode("Hello, World!") + "\n");      // 87cURD_*#4DfTZ)+T
    vector<byte> zeros(8);
    io::stdout.write(encoding::ascii85::encode(zeros) + "\n");                // zz
    auto back = encoding::ascii85::decode("87cURD_*#4\nDfTZ)+T");                      // white space skipped
    io::stdout.write(string(reinterpret_cast<const char*>(back->data()), back->size()) + "\n");   // Hello, World!
    io::stdout.write(encoding::ascii85::decode("s8W-\"").error().message() + "\n");   // offset 4: a group past 32 bits
}
```

## See also

[base64](base64.md); [`error`](error.md).
