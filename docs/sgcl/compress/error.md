# sgcl::compress::error

```cpp
#include "sgcl/compress/error.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    enum class errc : uint8_t {
        corrupt = 1, checksum, unexpected_end, unsupported, too_large,
        invalid_header, invalid_argument, dictionary_required, io,
        password_required, wrong_password, insecure_path
    };
    class error;
    const std::error_category& compress_category() noexcept;
}
```

The error of every format of the module, one type under each format's name (`flate::error`, `gzip::error`, `zip::error`): a value, copied, compared, held in an `expected`.

| code | what |
|---|---|
| `corrupt` | data the format does not allow: a Huffman code that is not one, a distance before the start, an entry longer than its directory says |
| `checksum` | a CRC-32, an Adler-32 or a bzip2 block's CRC does not match |
| `unexpected_end` | the data ends in the middle |
| `unsupported` | a zip method other than store and deflate, encryption, a sparse tar file |
| `too_large` | past a limit: the size decompressed (`limits`), a pax header, a 7z key of more than 2^24 rounds |
| `invalid_header` | a gzip, zip or tar header that cannot be read |
| `invalid_argument` | what a writer cannot write: a gzip name past ISO 8859-1, bytes past a tar entry's size |
| `dictionary_required` | a zlib stream made with a preset dictionary, read without it or with another |
| `io` | the source or the sink failed: `io_error()` says how |
| `password_required` | a 7z entry or header encrypted (7zAES), read without a password |
| `wrong_password` | 7z data encrypted under another password — or damaged, which decrypts alike |
| `insecure_path` | an entry extracted ([`sevenzip::extract`](sevenzip.md#extract)) whose name, or a link's target, would leave the directory, as Go's `ErrInsecurePath`: compress's code of `io::errc::insecure_path`, by the rule of [`io::path::is_local`](../io/path.md) |

## Members

```cpp
error();
error(errc code, uint64_t offset);
error(errc code, uint64_t offset, const string& detail);   // the detail said in place of the code's own words
error(const io::error& e, uint64_t offset);                 // errc::io, the stream's error kept
errc code() const noexcept;
uint64_t offset() const noexcept;                    // the byte of the compressed input (of the archive) where it was found
const optional<io::error>& io_error() const noexcept;
string message() const;                              // "offset 1234: gzip: CRC-32 mismatch"
friend bool operator==(const error&, const error&) noexcept;
```

A format that knows more says it in `message()`: a zip or tar error names its entry ("zip: entry a/b.txt: CRC-32 mismatch"). `make_error_code(errc)` and `compress_category()` make the `error_code` a stream's `io::error` carries.
