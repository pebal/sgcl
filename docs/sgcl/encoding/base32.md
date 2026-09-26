# sgcl::encoding::base32

```cpp
#include "sgcl/encoding/base32.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class base32;            // a codec: an alphabet, the padding or none, strict or lenient
    class base32::encoder;   // a writer and a closer (io::req::writer, io::req::closer)
    class base32::decoder;   // a reader (io::req::reader)
}
```

Base32 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) sections 6 and 7: five bytes as eight characters of an alphabet of 32, letters of one case and digits — a text a person reads aloud or types, a name on a file system that does not care about case, the secret of a one-time password. The same codec as [base64](base64.md) with five bits a character, and the same members: everything on that page holds here with 8 characters for 5 bytes.

## Rules

- The rules of [base64](base64.md#rules): strict by default and `lenient()` on request, a padded codec wanting its padding, the offsets of the errors, the sizes that never wrap, the streams.
- **The letters are upper case**, as the RFC writes them; a lower-case letter is outside the alphabet. A program that takes either case makes an alphabet of its own for the lower case or upper-cases the text first.
- **A short group without padding ends only where a byte ends**: after 2, 4, 5 or 7 characters. `MFR` (three characters, one byte and seven bits more) is refused at its end.
- **Where Go differs.** Go has no strict base32 and takes bits past the data; it drops a group of fewer than eight characters after a padded one, whatever it holds (`MY======M` is `f` there, refused here); it takes the short groups above for nothing and no error; and its codec without padding takes the byte `0xFF` for padding (its `NoPadding` is -1, and -1 as a byte is `0xFF`). The tests hold both sides to each of these by name.

## Members

```cpp
class base32 {
public:
    using error = encoding::error;
    class encoder;
    class decoder;

    static const encoding::base32 standard;   // RFC 4648 section 6: A-Z, 2-7
    static const encoding::base32 hex;        // section 7, "base32hex": 0-9, A-V, which keeps the order of the bytes

    constexpr encoding::base32(const char (&alphabet)[33], optional<char> padding = '=');
    // without_padding, lenient, padded, is_lenient, encode, decode, encoded_size,
    // max_decoded_size, encode_to, decode_to, encoder_to, decoder_from: as base64's
};
```

## Example

```cpp
#include "sgcl/encoding/base32.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    io::stdout.write(encoding::base32::standard.encode("foobar") + "\n");                     // MZXW6YTBOI======
    io::stdout.write(encoding::base32::hex.encode("foobar") + "\n");                          // CPNMUOJ1E8======
    // the secret of a one-time password: base32 without padding
    auto secret = encoding::base32::standard.without_padding().decode("JBSWY3DPEHPK3PXP");
    io::stdout.write(string(std::to_string(secret->size())) + " bytes\n");          // 10 bytes
    // strict: the bits past the data must be zero
    auto odd = encoding::base32::standard.decode("MZ======");
    io::stdout.write(odd.error().message() + "\n");                                 // offset 1: bits past the data in the last character
    io::stdout.write(string(std::to_string(encoding::base32::standard.lenient().decode("MZ======")->size())) + " byte\n");   // 1 byte
}
```

## See also

[base64](base64.md); [`error`](error.md).
