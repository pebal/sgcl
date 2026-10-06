[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::code

```cpp
string code() const;
```

The key's current code: [totp::generate](../totp/generate.md) of now for a TOTP key, [hotp::generate](../hotp/generate.md)
of `counter` for an HOTP key, with the key's secret and options. What the user's app shows.

## Parameters

None.

## Return value

The code, `options.digits` decimal digits.

## Complexity

Constant: one HMAC.

## Exceptions

`std::invalid_argument` when the key's options are out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key("otpauth://hotp/x?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&counter=0");
    println("{}", key.code());
}
```

Output:

```text
755224
```

## See also

- [verify](verify.md): the check
- [sgcl::crypto::otp_key](README.md)
