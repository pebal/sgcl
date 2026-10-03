[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::lenient

```cpp
constexpr base64 lenient() const noexcept;
```

The same codec with a lenient decoding: it skips `'\r'` and `'\n'` anywhere in the text, and takes any bits past
the data in the last character, which a strict decoding refuses (RFC 4648 sections 3.3 and 3.5). It reads the
text of MIME, which wraps its lines at 76 characters, and the PEM files of old encoders. This is what Go's
decoding is by default: a text Go reads, `lenient()` reads. Go's `Strict()` is the other way round, a strict
codec made from its lenient default.

Every other character outside the alphabet is still an error, and the encoding is the same: only the decoding
changes.

## Parameters

None.

## Return value

The codec with the lenient decoding.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto mime = encoding::base64::standard.lenient();
    println("{}", string(mime.decode("aGVs\r\nbG8=").value()));

    // QR== has bits past the data: refused, or taken
    println("{}", encoding::base64::standard.decode("QR==").error().message());
    println("{}", string(mime.decode("QR==").value()));
}
```

Output:

```text
hello
offset 1: bits past the data in the last character
A
```

## See also

- [is_lenient](is_lenient.md): whether a codec's decoding is lenient
- [decode](decode.md): the bytes of a text
- [sgcl::encoding::base64](../base64.md)
