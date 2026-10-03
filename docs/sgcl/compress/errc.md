[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::errc

```cpp
#include "sgcl/compress/error.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    enum class errc : uint8_t {
        corrupt = 1,
        checksum,
        unexpected_end,
        unsupported,
        too_large,
        invalid_header,
        invalid_argument,
        dictionary_required,
        io,
        password_required,
        wrong_password,
        insecure_path
    };
}
```

What went wrong in compressed data or in an archive: one list for every format of the module, as encoding has one
for its formats; a format uses the codes that mean something for it. The code of an [error](error.md) is one of
these ([code](error/code.md)). The values start at 1, since an `error_code` of 0 is success: `errc` is an error
code enumeration (`std::is_error_code_enum`), so a value converts to an `error_code` of the
[compress category](compress_category.md) ([make_error_code](make_error_code.md)), and that is what an `io::error`
carries when a reader of the module, read as an [io stream](../io/README.md), fails.

| Value | Description |
|---|---|
| `corrupt` | data the format does not allow: a Huffman code that is not one, a distance before the start, an entry longer than its directory says |
| `checksum` | a CRC-32, an Adler-32 or a bzip2 block's CRC does not match |
| `unexpected_end` | the data ends in the middle |
| `unsupported` | what the module does not read: a zip method other than its own, encryption in zip, a sparse tar file |
| `too_large` | past a limit: the size decompressed ([limits](limits.md)), a pax header, a 7z key of more than 2^24 rounds |
| `invalid_header` | a gzip, zip or tar header that cannot be read |
| `invalid_argument` | what a writer cannot write: a gzip name with a NUL, bytes past a tar entry's size |
| `dictionary_required` | a zlib stream made with a preset dictionary, read without it or with another |
| `io` | the source or the sink failed: the error's [io_error](error/io_error.md) says how |
| `password_required` | a 7z entry or header encrypted (7zAES), read without a password |
| `wrong_password` | 7z data encrypted under another password — or damaged, which decrypts alike |
| `insecure_path` | an entry extracted whose name, or a link's target, would leave the directory, as Go's `ErrInsecurePath`: compress's code of `io::errc::insecure_path`, by the rule of [io::path::is_local](../io/path.md) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::gzip::compress("hello, hello, hello");
    packed[packed.size() - 8] ^= byte(1);  // a bit of the CRC-32 flipped

    auto back = compress::gzip::decompress(packed);
    println("{}", back.error().code() == compress::errc::checksum);

    auto cut = compress::gzip::decompress(slice<const byte>(packed).first(10));
    println("{}", cut.error().code() == compress::errc::unexpected_end);
}
```

Output:

```text
true
true
```

## See also

- [error](error.md): the code, the offset and the message
- [make_error_code](make_error_code.md), [compress_category](compress_category.md): the code as an `error_code`
- [sgcl::compress](README.md)
