# sgcl::encoding::base64

```cpp
#include "sgcl/encoding/base64.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class base64;            // a codec: an alphabet, the padding or none, strict or lenient
    class base64::encoder;   // a writer and a closer (io::req::writer, io::req::closer)
    class base64::decoder;   // a reader (io::req::reader)
}
```

Base64 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) sections 4 and 5: three bytes as four characters of an alphabet of 64. A codec is a value — the alphabet, the padding or none, strict or lenient — with `encode` and `decode` as its methods; the four alphabets Go names are constants, and an alphabet of one's own is a constructor.

## Rules

- **Strict by default.** A character outside the alphabet is an error, a line ending included (section 3.3), and so are bits past the data in the last character (section 3.5): `QR==` is not `QQ==` written another way, it is refused, since two texts that decode to the same bytes are what a signature or a key cannot have. `lenient()` skips `'\r'` and `'\n'` anywhere and takes the bits — MIME, PEM files of old encoders — and is what Go reads by default.
- **A padded codec wants its padding, a codec without padding refuses it.** `standard` reads `Zg==` and refuses `Zg`; `raw_standard` reads `Zg` and refuses `Zg==`.
- **Where an error is.** The offset is where the text stops being the start of valid base64: the character outside the alphabet, the padding where the data cannot end (`Q=`), the first character that is not padding inside the padding (`QQ=x`: 3), the first character after it (`QQ==x`: 4), the character with bits past the data (`QR==`: 1), the end of a cut text (`QQ`, `QQ=`: the length). Go names the start of the group for a cut text, and the padding for the other two; the tests hold both sides to these cases by name.
- **Sizes never wrap.** `encoded_size(n)` of an `n` no `size_t` can hold the characters of is `SIZE_MAX`, the size no buffer has; `encode_to` and `decode_to` throw `length_error` on a buffer smaller than `encoded_size` or `max_decoded_size` — a mistake in the program, not in the input. A text longer than a `string` holds (4 G characters) is `length_error` too.
- **An alphabet of one's own** is 64 different characters, none of them `'\0'`, `'\r'`, `'\n'` or the padding, in an array of 65 whose last is the terminator — the length is the array's, the characters past it are never read; anything else is `invalid_argument`, which in a constant is an error at compile time.
- **The streams.** `encoder_to(w)` is a writer that writes the encoding of what it is given to `w`: whole groups at once, the bytes short of a group kept for the next write, and `close()` writes the last group with its padding — and leaves `w` open, as Go's does, since what is written around the base64 usually goes on. A failure of `w` is kept for good, as Go's encoder keeps it: the group being written went with it, so every later `write` and `close` reports that failure rather than going on without those bytes. `decoder_from(r)` is a reader of the bytes `r`'s text decodes to; the text may come in pieces of any size, and a read into a buffer smaller than a group gets the group through the decoder. An invalid text fails the read that reaches it, after the bytes before the error were handed out, and every read after; the read's `io::error` has the code in the `encoding` category, and `last_error()` holds the [`error`](error.md) with its offset in the text.

## Members

```cpp
class base64 {
public:
    using error = encoding::error;
    class encoder;
    class decoder;

    static const encoding::base64 standard;        // RFC 4648 section 4, '='
    static const encoding::base64 url;             // section 5 ('-' and '_'), '='
    static const encoding::base64 raw_standard;    // section 4, no padding
    static const encoding::base64 raw_url;         // section 5, no padding (a JWT's)

    constexpr encoding::base64(const char (&alphabet)[65], optional<char> padding = '=');
    constexpr encoding::base64 without_padding() const noexcept;
    constexpr encoding::base64 lenient() const noexcept;
    constexpr bool padded() const noexcept;
    constexpr bool is_lenient() const noexcept;

    string encode(const slice<const byte>& data) const;
    string encode(const string& text) const;                         // the bytes of the text
    expected<vector<byte>, error> decode(const string& text) const;

    constexpr size_t encoded_size(size_t n) const noexcept;          // SIZE_MAX when it does not fit
    constexpr size_t max_decoded_size(size_t n) const noexcept;
    size_t encode_to(const slice<char>& out, const slice<const byte>& data) const;          // the characters written
    expected<size_t, error> decode_to(const slice<byte>& out, const string& text) const;   // the bytes written

    tracked_ptr<encoder> encoder_to(const io::writer& out) const;
    tracked_ptr<decoder> decoder_from(const io::reader& in) const;
};

class base64::encoder final {   // and everything of io::mixin::writer
    expected<size_t, io::error> write(const slice<const byte>& data);
    async::task<expected<size_t, io::error>> async_write(slice<const byte> data);
    expected<void, io::error> close();  async::task<expected<void, io::error>> async_close();   // the last group; the writer under it stays open
    bool is_closed() const noexcept;
};

class base64::decoder final {   // and everything of io::mixin::reader
    expected<size_t, io::error> read(const slice<byte>& buffer);
    async::task<expected<size_t, io::error>> async_read(slice<byte> buffer);
    const optional<error>& last_error() const noexcept;
};
```

`max_decoded_size(n)` of a padded codec counts every group the text starts as whole — `(n + 3) / 4 * 3` — so a buffer that size holds whatever a text of `n` characters decodes to before it is found wrong; without padding it is the bits of `n` characters, `n * 6 / 8`.

## Example

```cpp
#include "sgcl/encoding/base64.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    // Basic authentication: the bytes of a text
    string header = "Basic " + encoding::base64::standard.encode("ala:sekret");
    io::stdout.write(header + "\n");                      // Basic YWxhOnNla3JldA==

    // A JWT's segment: the URL alphabet without padding
    auto claims = encoding::base64::raw_url.decode("eyJzdWIiOiI0MiJ9");
    if (claims) {
        io::stdout.write(string(reinterpret_cast<const char*>(claims->data()), claims->size()) + "\n");   // {"sub":"42"}
    }

    // Strict by default: a line ending is not base64
    auto wrapped = encoding::base64::standard.decode("YWxh\nOnNla3JldA==");
    io::stdout.write(wrapped.error().message() + "\n");   // offset 4: invalid character 0x0A
    // MIME wraps its lines: lenient() skips them
    auto mime = encoding::base64::standard.lenient().decode("YWxh\r\nOnNla3JldA==");
    io::stdout.write(string(std::to_string(mime->size())) + " bytes\n");   // 10 bytes

    // A stream: what is written goes out encoded
    tracked_ptr out = make_tracked<io::buffer>();
    auto enc = encoding::base64::standard.encoder_to(out);
    enc->write("hello, ");
    enc->write("world");
    enc->close();// the last group and its padding
    io::stdout.write(out->text() + "\n");                 // aGVsbG8sIHdvcmxk
}
```

An alphabet of one's own — `crypt(3)`'s, which puts `./` first and has no padding:

```cpp
static constexpr encoding::base64 crypt("./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", nullopt);
```

## The loops

Encoding reads a group of three bytes into a word and writes four characters from a table of 64; decoding reads four characters through a table of 256 into a word and writes three bytes, and leaves the loop only when a group holds a byte outside the alphabet — a line ending, the padding or an error — which the rest of the decoder then takes a character at a time. There is no branch in either loop but that one, and no intrinsic: the loops are scalar, as Go's are, and the compiler is left to them.

## See also

[base32](base32.md), the same codec with five bits a character; [pem](pem.md), base64 between two lines; [`error`](error.md); [io streams](../io/stream.md).
