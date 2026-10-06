[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::otp_options

```cpp
#include "sgcl/crypto/otp.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    struct otp_options {
        hash_id algorithm = hash_id::sha1;
        uint32_t digits = 6;
        uint32_t period = 30;
        uint32_t skew = 1;
    };
}
```

How a one-time password of [hotp](hotp/README.md) and [totp](totp/README.md) is made and checked: the HMAC's digest,
the length of a code, the period of TOTP, and how far a check looks. The defaults are those of RFC 4226, RFC 6238 and
every authenticator app: HMAC-SHA-1, six digits, thirty seconds. An option out of its range is a broken contract,
`std::invalid_argument`, from every function that takes it.

## Member objects

| Member | Description |
|---|---|
| `algorithm` | the [hash_id](hash_id.md) of the HMAC: `sha1` (the default and what most apps take), `sha256` or `sha512` |
| `digits` | the length of a code, 6 to 10; 6 by default |
| `period` | TOTP: the seconds a code lives, 1 or more; 30 by default |
| `skew` | a check: TOTP periods accepted either side of now, for a clock that is off; HOTP counters accepted ahead of the expected one, for codes generated and never used; 1 by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 6238 Appendix B: SHA-256, eight digits, at 59 seconds past the epoch
    crypto::otp_options o{.algorithm = crypto::hash_id::sha256, .digits = 8};
    println("{}", crypto::totp::generate("12345678901234567890123456789012", time::datetime::from_unix(59), o));
}
```

Output:

```text
46119246
```

## See also

- [hotp](hotp/README.md), [totp](totp/README.md): what takes them
- [otp_key](otp_key/README.md): a key with its options, as an app has it
- [The module](README.md)
