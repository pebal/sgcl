# sgcl::codec::error, errc

```cpp
#include "sgcl/codec/error.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    enum class errc : uint8_t {
        corrupt = 1,         // data the format does not allow
        checksum,            // a PNG chunk's CRC-32 or the zlib stream's Adler-32 does not match
        unexpected_end,      // the data ends in the middle
        unsupported,         // valid data the module does not read, or no format it knows
        too_large,           // past limits: the pixels, the metadata
        invalid_argument,    // decode_options.want outside the list
        io                   // the stream failed: io_error() says how
    };

    class error {
    public:
        errc code() const noexcept;
        uint64_t offset() const noexcept;                    // bytes from the start of the file
        const optional<io::error>& io_error() const noexcept;
        string message() const;                              // "offset 16: png: IHDR width 0"
    };
    const std::error_category& codec_category() noexcept;   // an errc is a std::error_code of it
}
```

What went wrong in an image file: one list of codes for every format, the byte of the file where it was found, the error of the stream when the file came from one and that failed, and `message()` with the format's own words.

- **A value.** It can be copied, compared and held in an `expected`. Read through an `io::reader`, a decoder's failure is an `io::error` of the `codec` category.
- **Codes.** They start at 1: an `error_code` of 0 is success.

## Members

### errc

```cpp
enum class errc : uint8_t { corrupt = 1, checksum, unexpected_end, unsupported, too_large, invalid_argument, io };
```

The kinds of failure, one list for every format, as the names block above says each. They start at 1, so that an `error_code` of 0 is success.

### error

```cpp
errc code() const noexcept;
uint64_t offset() const noexcept;                       // bytes from the start of the file
const optional<io::error>& io_error() const noexcept;
string message() const;                                 // "offset 16: png: IHDR width 0"
```

The code, the byte of the file where the failure was found, and the stream's own error when the code is `errc::io`. `message()` puts the offset and the format's words together.

### codec_category

```cpp
const std::error_category& codec_category() noexcept;
```

The category of an `errc` as a `std::error_code`, and of the `io::error` a decoder read through an `io::reader` gives.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/png_decode/basn2c08.png");
    auto whole = codec::png::decode(file);
    println("whole: {}", whole.has_value());
    auto cut = codec::png::decode(slice<const byte>(file.data(), 100));
    if (!cut) {
        println("{} ({})", cut.error().message(),
                cut.error().code() == codec::errc::unexpected_end);
    }
    file[29] = byte(std::to_integer<int>(file[29]) ^ 1);  // a byte of IHDR's CRC-32
    auto damaged = codec::png::decode(file);
    if (!damaged) println(damaged.error().message());
}
```

Output:

```text
whole: true
offset 100: png: the data ends in the middle (true)
offset 8: png: CRC-32 of chunk IHDR
```

## See also

[`decode`](decode.md), [`io::error`](../io/error.md).
