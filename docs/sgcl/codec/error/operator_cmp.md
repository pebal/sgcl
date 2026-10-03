[sgcl](../../README.md) › [codec](../README.md) › [error](../error.md)

# sgcl::codec::operator== (sgcl::codec::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors by what they say: the same code at the same byte, in the same words, and the same stream's error
when there is one. `!=` is made from it by the compiler. The stream's errors are compared as
[io::error](../../io/error.md) compares them, by their codes alone: two failed reads of different files with the
same code at the same byte are equal.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors compared |

## Return value

`true` when the codes, the offsets, the words and the stream's errors are equal.

## Complexity

Linear in the length of the words.

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
    auto a = codec::png::decode(file.as_slice(0, 40));
    auto b = codec::png::decode(file.as_slice(0, 40));
    auto c = codec::png::decode(file.as_slice(0, 41));
    println("{} {}", a.error() == b.error(), a.error() == c.error());
    println("{}", codec::error(codec::errc::corrupt, 0) == codec::error());
}
```

Output:

```text
true false
false
```

## See also

- [code](code.md), [offset](offset.md), [message](message.md): what is compared
- [sgcl::codec::error](../error.md)
