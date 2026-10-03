[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::lenient

```cpp
constexpr base32 lenient() const noexcept;
```

The same codec with a lenient decoding: it skips `'\r'` and `'\n'` anywhere in the text, and takes any bits past
the data in the last character, which a strict decoding refuses (RFC 4648 sections 3.3 and 3.5). Go's base32
has no strict decoding: it takes the bits always. Every other character outside the alphabet is still an error,
the groups Go takes and this refuses ([From code written for Go](../base32.md#from-code-written-for-go)) are
refused here too, and the encoding is the same: only the decoding changes.

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
    auto loose = encoding::base32::standard.lenient();
    println("{}", string(loose.decode("MZXW6\r\n===").value()));
    println("{}", string(loose.decode("MZ======").value()));
    println("{}", loose.decode("MY======M").error().message());
}
```

Output:

```text
foo
f
offset 8: data after the padding: 'M'
```

## See also

- [is_lenient](is_lenient.md): whether a codec's decoding is lenient
- [decode](decode.md): the bytes of a text
- [sgcl::encoding::base32](../base32.md)
