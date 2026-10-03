[sgcl](../../README.md) › [codec](../README.md) › [error](README.md)

# sgcl::codec::error::message

```cpp
string message() const noexcept;
```

The error as one sentence for a person: `"offset "`, the [offset](offset.md), `": "`, and the words of the failure,
the format's own when it gave them (`"offset 8: png: CRC-32 of chunk IHDR"`) and the code's otherwise
(`"offset 8: size limit exceeded"`); for `errc::io`, `": "` and the stream's own message after them
(`"offset 0: input/output error: open photo.png: No such file or directory"`). A default error, of no code, says
`"no error"`. It is also what `what()` of a [bad_expected_access](../../core/bad_expected_access/README.md)`<codec::error>`
says.

## Parameters

None.

## Return value

The sentence, as a `string`.

## Complexity

Linear in the length of the sentence.

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
    println("whole: {}", codec::png::decode(file).has_value());
    println(codec::png::decode(file.as_slice(0, 40)).error().message());
    file[29] = byte(std::to_integer<int>(file[29]) ^ 1);  // a byte of IHDR's CRC-32
    println(codec::png::decode(file).error().message());
    try {
        codec::image read = codec::png::decode(file);
    } catch (const std::exception& e) {
        println(e.what());
    }
}
```

Output:

```text
whole: true
offset 40: png: the data ends in the middle
offset 8: png: CRC-32 of chunk IHDR
offset 8: png: CRC-32 of chunk IHDR
```

## See also

- [code](code.md): the kind of failure, for a program
- [offset](offset.md): the byte
- [sgcl::codec::error](README.md)
