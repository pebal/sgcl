[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::errc

```cpp
#include "sgcl/codec/error.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    enum class errc : uint8_t {
        corrupt = 1,
        checksum,
        unexpected_end,
        unsupported,
        too_large,
        invalid_argument,
        io
    };
}

template<>
struct std::is_error_code_enum<sgcl::codec::errc> : std::true_type {};
```

What went wrong in an image file, one list for every format of the module: a format uses the codes that mean
something for it. An [error](error.md) carries one as its [code](error/code.md), beside the byte it was found at,
and a program tells one failure from another by it.

The values start at 1, so that an `error_code` of 0 is success: `std::is_error_code_enum` is specialized, and an
`errc` converts to a `std::error_code` of [codec_category](codec_category.md) ([make_error_code](make_error_code.md))
and compares with one. The words of each code are the category's, and what [error::message](error/message.md) says
when the format gave no words of its own.

| Value | Description |
|---|---|
| `corrupt` | 1, data the format does not allow: a bad chunk, a Huffman code with no symbol, a row past the image, a side of zero pixels; "corrupt image data" |
| `checksum` | 2, a PNG chunk's CRC-32 or the zlib stream's Adler-32 does not match; "checksum mismatch" |
| `unexpected_end` | 3, the data ends in the middle; "unexpected end of data" |
| `unsupported` | 4, valid data the module does not read (arithmetic-coded JPEG, 12-bit samples), data that is no format the module reads, a path whose extension is no format the module writes, HEIF on a system without its codec; "unsupported feature" |
| `too_large` | 5, more than the [limits](limits.md) allow: the pixels, the metadata; "size limit exceeded" |
| `invalid_argument` | 6, what the program asked makes no sense: a [decode_options](decode_options.md)`.want` outside the list of [pixel_format](pixel_format.md), for every decoder; a HEIC quality outside 1 to 100; an image the format cannot hold (a JPEG side past 65 535 pixels, a PNG side past 2^31 − 1) or the system's encoder does not take. JPEG's quality outside 1 to 100 and its subsampling outside the list are a `std::invalid_argument` thrown instead; "invalid argument" |
| `io` | 7, the source or the sink failed, a file that does not open, read, write or rename: [io_error](error/io_error.md) says how; "input/output error" |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(4, 4, codec::pixel_format::rgb8);
    vector<byte> file = codec::png::encode(picture);
    auto cut = codec::png::decode(file.as_slice(0, 40));
    println("{}", cut.error().code() == codec::errc::unexpected_end);
    auto gray = codec::png::decode(file, {.want = codec::pixel_format(9)});
    println("{}", gray.error().code() == codec::errc::invalid_argument);
    std::error_code code = codec::errc::checksum;
    println("{}: {} {}", code.category().name(), code.value(), code.message());
}
```

Output:

```text
true
true
codec: 2 checksum mismatch
```

## See also

- [error](error.md): the code, the byte and the words
- [codec_category](codec_category.md), [make_error_code](make_error_code.md): an `errc` as a `std::error_code`
- [sgcl::codec](README.md)
