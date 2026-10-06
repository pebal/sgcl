[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::to_string

```cpp
string to_string() const;
```

The key's otpauth:// URI: the type, the label `issuer:account` (the account alone without an issuer) percent-encoded,
the secret in base32 without padding, the issuer again as a parameter, then the algorithm, the digits and the period
when they are not the defaults (`SHA1`, 6, 30), and an HOTP key's counter. [parse](parse.md) reads it back to the same
key. The text holds the secret: it is what goes into a QR code for the user's app.

## Parameters

None.

## Return value

The URI.

## Complexity

Linear in the lengths of the secret, the issuer and the account.

## Exceptions

`std::invalid_argument` when the key's options are out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key;
    key.secret = crypto::secret_bytes(10);
    key.issuer = "Example";
    key.account = "alice smith";
    key.options.digits = 8;
    println("{}", key.to_string());
}
```

Output:

```text
otpauth://totp/Example:alice%20smith?secret=AAAAAAAAAAAAAAAA&issuer=Example&digits=8
```

## See also

- [parse](parse.md): the other way
- [sgcl::crypto::otp_key](README.md)
