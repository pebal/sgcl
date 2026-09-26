# sgcl::encoding::hex

```cpp
#include "sgcl/encoding/hex.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class hex;           // encode, encode_upper, decode, dump
    class hex::encoder;  // a writer and a closer (io::req::writer, io::req::closer)
    class hex::decoder;  // a reader (io::req::reader)
    class hex::dumper;   // a writer and a closer (io::req::writer, io::req::closer)
}
```

Hexadecimal, base16 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) section 8: a byte as two digits. Nothing to choose, so every member is static. `dump` is the view `hexdump -C` gives — sixteen bytes a line and their characters — for a log, a test, or what came over a wire.

## Rules

- `encode` writes lower-case digits, `encode_upper` upper-case ones; `decode` takes either case, as the section allows a decoder to.
- `decode` refuses the first character that is not a digit (`invalid_character` at its offset) and an odd length (`unexpected_end` at the end); nothing else can be wrong. Go refuses the same texts, its error naming the byte rather than its offset.
- `dump` writes Go's `hex.Dump` line for line: the offset in eight hexadecimal digits (more past 4 GB), two spaces, sixteen bytes in two columns of eight, and the bytes as characters between bars, a dot for what is not printable ASCII; the short line at the end keeps the columns where they are. There is no closing line with the total that `hexdump -C` writes.
- The sizes, the caller's buffers and the streams are as [base64's](base64.md#rules): `encode_to` and `decode_to` allocate nothing and are `length_error` into a buffer too small, `encoder_to(w)` writes lower-case digits and leaves `w` open at `close()`, `decoder_from(r)` takes its digits in pieces of any size.
- `dumper_to(w)` writes a line to `w` as soon as its sixteen bytes are there; `close()` writes the short line and leaves `w` open. A failure of `w` is kept for good: every later `write` and `close` reports it.

## Members

```cpp
class hex {
public:
    using error = encoding::error;
    class dumper;

    static string encode(const slice<const byte>& data);           // lower case
    static string encode(const string& text);                    // the bytes of the text
    static string encode_upper(const slice<const byte>& data);
    static string encode_upper(const string& text);
    static expected<vector<byte>, error> decode(const string& text);   // either case
    static constexpr size_t encoded_size(size_t n) noexcept;           // 2n
    static constexpr size_t max_decoded_size(size_t n) noexcept;       // n / 2
    static size_t encode_to(const slice<char>& out, const slice<const byte>& data);                // into the caller's buffer
    static expected<size_t, error> decode_to(const slice<byte>& out, const string& text);
    static tracked_ptr<encoder> encoder_to(const io::writer& out);     // lower case, as base64's streams
    static tracked_ptr<decoder> decoder_from(const io::reader& in);
    static string dump(const slice<const byte>& data);
    static string dump(const string& text);
    static tracked_ptr<dumper> dumper_to(const io::writer& out);
};

class hex::dumper final : public io::mixin::writer<hex::dumper> {
    expected<size_t, io::error> write(const slice<const byte>& data);
    async::task<expected<size_t, io::error>> async_write(slice<const byte> data);
    expected<void, io::error> close();  async::task<expected<void, io::error>> async_close();   // the short line; the writer under it stays open
    bool is_closed() const noexcept;
};
```

## Example

```cpp
#include "sgcl/encoding/hex.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    vector<byte> digest = {byte(0xDE), byte(0xAD), byte(0xBE), byte(0xEF)};
    io::stdout.write(encoding::hex::encode(digest) + "\n");          // deadbeef
    io::stdout.write(encoding::hex::encode_upper(digest) + "\n");    // DEADBEEF
    auto back = encoding::hex::decode("DeadBeef");                           // either case
    io::stdout.write(string(std::to_string(back->size())) + " bytes\n");   // 4 bytes
    io::stdout.write(encoding::hex::decode("abc").error().message() + "\n");         // offset 3: the input ends inside a byte
    io::stdout.write(encoding::hex::decode("0x12").error().message() + "\n");        // offset 1: invalid character 'x'

    io::stdout.write(encoding::hex::dump("Hello, World!\n"));
    // 00000000  48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a        |Hello, World!.|

    // what goes over a wire, dumped as it goes
    auto d = encoding::hex::dumper_to(io::stdout);
    d->write("a line of twenty bytes");
    d->close();// the short line at the end
}
```

```
00000000  61 20 6c 69 6e 65 20 6f  66 20 74 77 65 6e 74 79  |a line of twenty|
00000010  20 62 79 74 65 73                                 | bytes|
```

## See also

[base64](base64.md), [base32](base32.md), the same codec with more bits a character; [`error`](error.md).
