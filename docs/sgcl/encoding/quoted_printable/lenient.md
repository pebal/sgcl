[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::lenient

```cpp
constexpr quoted_printable lenient() const noexcept;
```

The same codec in a lenient decoding: a `=` that is neither an escape nor a soft break stands for itself, and an escape cut short by the end too; the robust decoder of RFC 2045 §6.7, Python's `quopri`. A lenient decoding never fails. The other choice, [binary](binary.md), stays as it was.

## Parameters

None.

## Return value

The codec with the choice made.

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
    auto robust = encoding::quoted_printable::standard.lenient();
    println("{}", string(robust.decode("price = 5=E2=82=AC, 100%=").value()));
    println("{}", encoding::quoted_printable::standard.decode("=4").error().message());
    println("{}", string(robust.decode("=4").value()));
}
```

Output:

```text
price = 5€, 100%
offset 2: quoted-printable escape cut short
=4
```

## See also

- [is_lenient](is_lenient.md)
- [binary](binary.md)
- [quoted_printable](README.md)
