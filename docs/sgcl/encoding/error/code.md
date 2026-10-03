[sgcl](../../README.md) › [encoding](../README.md) › [error](README.md)

# sgcl::encoding::error::code

```cpp
errc code() const noexcept;
```

What went wrong, one [errc](../errc.md) of the list every format of the module shares: a program handling the
errors of several formats does it with one `switch`. A format uses the codes that mean something for it and no
other.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

string why(const encoding::error& e) {
    switch (e.code()) {
        case encoding::errc::invalid_character:
            return "a character that does not belong";
        case encoding::errc::unexpected_end:
            return "cut short";
        default:
            return "malformed";
    }
}

int main() {
    println("{}", why(encoding::base64::standard.decode("QU*D").error()));
    println("{}", why(encoding::hex::decode("abc").error()));
    println("{}", why(encoding::pem::parse("-----BEGIN A-----\nQUJD\n-----END B-----\n").error()));
}
```

Output:

```text
a character that does not belong
cut short
malformed
```

## See also

- [errc](../errc.md): the codes and the formats that raise them
- [message](message.md): the code's words, or the format's
- [sgcl::encoding::error](README.md)
