[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::generate

```cpp
static otp_key generate(const string& issuer, const string& account, const otp_options& o = {});
```

A new TOTP key for `account` at `issuer`: 20 random bytes of secret ([random::secret](../random/secret.md), RFC 4226's
160 bits), the options `o`. Its [to_string](to_string.md) goes to the user's app, in a QR code; the server keeps the
key, encrypted.

## Parameters

| Parameter | Description |
|---|---|
| `issuer` | the service, as the app shows it |
| `account` | the user's account, as the app shows it |
| `o` | the algorithm, the digits, the period ([otp_options](../otp_options.md)) |

## Return value

The key.

## Complexity

Constant.

## Exceptions

`std::invalid_argument` when `o` is out of range.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key = crypto::otp_key::generate("Example", "alice@example.com");
    println("{}", key.to_string());
}
```

Sample output:

```text
otpauth://totp/Example:alice@example.com?secret=L5KTR6Y3UQGK5HXTMY3YIZ5BXHQ2UGBX&issuer=Example
```

## See also

- [to_string](to_string.md): the URI
- [sgcl::crypto::otp_key](README.md)
