[sgcl](../../README.md) › [codec](../README.md) › [error](README.md)

# sgcl::codec::error::code

```cpp
errc code() const noexcept;
```

The kind of failure, from the one list of every format ([errc](../errc.md)): what a program tells one failure from
another by, where [message](message.md) is for a person to read. A file cut short is `errc::unexpected_end` in PNG
and JPEG alike, a damaged one `errc::corrupt` or `errc::checksum`. A default error's code is `errc{}`, 0, none of
the list.

## Parameters

None.

## Return value

The code.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

string verdict(const codec::error& e) {
    switch (e.code()) {
        case codec::errc::unexpected_end: return "cut short, try again";
        case codec::errc::corrupt:
        case codec::errc::checksum: return "damaged";
        default: return "not an image to use";
    }
}

int main() {
    codec::image picture(4, 4, codec::pixel_format::rgb8);
    vector<byte> file = codec::png::encode(picture);
    println(verdict(codec::png::decode(file.as_slice(0, 40)).error()));
    file[29] = byte(std::to_integer<int>(file[29]) ^ 1);  // a byte of IHDR's CRC-32
    println(verdict(codec::png::decode(file).error()));
    println(verdict(codec::decode(file.as_slice(4, 10)).error()));
}
```

Output:

```text
cut short, try again
damaged
not an image to use
```

## See also

- [errc](../errc.md): the codes
- [message](message.md): the words
- [sgcl::codec::error](README.md)
