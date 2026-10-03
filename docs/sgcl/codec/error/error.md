[sgcl](../../README.md) › [codec](../README.md) › [error](../error.md)

# sgcl::codec::error::error

```cpp
error() noexcept = default;                                          // (1)
error(errc code, uint64_t offset) noexcept;                          // (2)
error(errc code, uint64_t offset, const string& detail) noexcept;    // (3)
error(const io::error& e, uint64_t offset) noexcept;                 // (4)
error(const error& other) noexcept;                                  // (5)
error(error&& other) noexcept;                                       // (6)
```

Constructs an error. The library makes the errors of its decoders and encoders; a program makes one for a reading of
its own that reports as the module does, in the same `expected`.

1. No code of the list, `errc{}` (0), at byte 0: what an `error` is before anything is assigned to it.
   [message](message.md) says `"no error"`.
2. The code `code` at the byte `offset`; [message](message.md) says the code's own words.
3. The code `code` at the byte `offset`, with the words `detail`, which [message](message.md) says in place of the
   code's: `"png: CRC-32 of chunk IHDR"`, `"jpeg: arithmetic coding"`.
4. `errc::io` at the byte `offset`: the source or the sink failed with `e`, which [io_error](io_error.md) gives
   back.
5. A copy of `other`, made without a throw.
6. `other` moved in.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the kind of failure |
| `offset` | the byte of the input where it was found |
| `detail` | the words of the failure, the format's name first |
| `e` | the error of the stream |
| `other` | the error to copy or move |

## Complexity

Constant: the words and the stream's error are shared, not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<codec::image, codec::error> read_ppm(const slice<const byte>& data) {
    if (data.size() < 2 || data[0] != byte('P') || data[1] != byte('6')) {
        return unexpected(codec::error(codec::errc::unsupported, 0, "ppm: P6 expected"));
    }
    return unexpected(codec::error(codec::errc::unexpected_end, data.size()));
}

int main() {
    vector<byte> file = codec::png::encode(codec::image(1, 1, codec::pixel_format::gray8));
    println(read_ppm(file).error().message());
    vector<byte> header = {byte('P'), byte('6')};
    println(read_ppm(header).error().message());
    println(codec::error().message());
}
```

Output:

```text
offset 0: ppm: P6 expected
offset 2: unexpected end of data
no error
```

## See also

- [message](message.md): what the error says
- [errc](../errc.md): the codes
- [sgcl::codec::error](../error.md)
