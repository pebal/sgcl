[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::otp_key

```cpp
#include "sgcl/crypto/otp.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    struct otp_key {
        otp_type type = otp_type::totp;
        secret_bytes secret;
        string issuer;
        string account;
        otp_options options;
        uint64_t counter = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::otp_key` is a one-time-password key as an authenticator app takes it: the secret, who issued it, the
account it is for, the options, and for HOTP the counter. Its text is the otpauth:// URI of Google Authenticator's Key
Uri Format, `otpauth://totp/Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example`, which a QR code carries
to the app: [to_string](to_string.md) writes it and [parse](parse.md) reads it, from any app that writes the format.
[generate](generate.md) makes a new key, [code](code.md) and [verify](verify.md) are [totp](../totp/README.md)'s or
[hotp](../hotp/README.md)'s with the key's own secret and options.

## Rules

- **The secret is a [secret_bytes](../secret_bytes/README.md)**, so the key is move-only and [clone](clone.md) is its
  copy. Its URI holds the secret too, in base32, since that is what a QR code carries: a `string` in managed memory,
  for the moment it is shown or stored, encrypted, by the server.
- **A URI is read as the apps read it**: the scheme and the type in any case, the label `issuer:account` split at its
  first colon (literal or `%3A`) or where the issuer parameter says, spaces after the colon dropped, the secret in
  base32 of any case with or without its padding, parameters it does not know skipped. It is written in the
  canonical form: the issuer in the label and as a parameter, the algorithm, the digits and the period only when they
  are not the defaults.
- **The skew** of the options is the server's and is not in the URI.

## Member objects

| Member | Description |
|---|---|
| `type` | [otp_type](../otp_type.md)`::totp` (the default) or `hotp` |
| `secret` | the shared secret |
| `issuer` | the service, as the app shows it; may be empty |
| `account` | the user's account, as the app shows it |
| `options` | the algorithm, the digits, the period ([otp_options](../otp_options.md)) |
| `counter` | HOTP: the next counter; 0 by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](otp_key.md) | an empty key, or one read from a URI |
| [operator=](operator_assign.md) | takes another key over |
| [clone](clone.md) | a copy, made on purpose |
| [generate](generate.md) | a new TOTP key with a random secret (static) |
| [parse](parse.md) | a key read from an otpauth:// URI (static) |
| [to_string](to_string.md) | the otpauth:// URI |
| [code](code.md) | the code now (TOTP) or at the counter (HOTP) |
| [verify](verify.md) | the time step or counter a code matches |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example");
    println("{} {} {}", key.issuer, key.account, key.secret.size());
    println("{}", key.verify(key.code()).has_value());
}
```

Output:

```text
Example alice@google.com 10
true
```

## See also

- [totp](../totp/README.md), [hotp](../hotp/README.md): the codes
- [The module](../README.md)
