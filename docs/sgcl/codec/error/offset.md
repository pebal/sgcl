[sgcl](../../README.md) › [codec](../README.md) › [error](../error.md)

# sgcl::codec::error::offset

```cpp
uint64_t offset() const noexcept;
```

The byte of the input where the failure was found, from the start of the input: the header or the field that failed,
the place the data ended, the chunk whose checksum does not match. For a stream it counts the bytes read, or written,
before. It is 0 where the failure has no byte of its own: a file that does not open, an extension
[save](../image/save.md) does not write, a [decode_options](../decode_options.md)`.want` outside the list.

## Parameters

None.

## Return value

The offset from the start of the input, in bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(4, 4, codec::pixel_format::rgb8);
    vector<byte> file = codec::png::encode(picture);
    println("{}", codec::png::decode(file.as_slice(0, 40)).error().offset());
    file[29] = byte(std::to_integer<int>(file[29]) ^ 1);  // a byte of IHDR's CRC-32
    println("{}", codec::png::decode(file).error().offset());
}
```

Output:

```text
40
8
```

## See also

- [code](code.md): what failed
- [message](message.md): the offset and the words, as one sentence
- [sgcl::codec::error](../error.md)
