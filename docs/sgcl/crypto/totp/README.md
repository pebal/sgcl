[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::totp

```cpp
#include "sgcl/crypto/otp.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class totp;
}
```

`sgcl::crypto::totp` is TOTP of RFC 6238: the [hotp](../hotp/README.md) code of the number of periods, 30 seconds by
default, since the Unix epoch — the six digits an authenticator app shows and a two-factor login asks for. Both sides
know the time, so neither counts; [verify](verify.md) accepts the codes of a period or more either side of now, for a
clock that is off, and gives the time step that matched.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A code is good once** (RFC 6238 §5.2): a server keeps the last time step it accepted for an account and refuses a
  match at or below it, which [verify](verify.md)'s result is for.
- **The secret** is at least 16 bytes and better 20, as for HOTP; an [otp_key](../otp_key/README.md) holds it with its
  options, and its URI goes to the app in a QR code.
- **The time** is the system's clock by default, or a [time::datetime](../../time/datetime/README.md) given; one before
  the Unix epoch cannot have a code: `std::invalid_argument` from generate, nothing from verify.
- **In constant time**: every code of the window is computed and compared, with no early exit.
- **Options out of range** are `std::invalid_argument`.

## Member functions

| Function | Description |
|---|---|
| [generate](generate.md) | the code of now, or of a time (static) |
| [verify](verify.md) | the time step whose code a given code is, now or at a time, within the skew (static) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 6238 Appendix B: SHA-1, eight digits
    time::datetime at = time::datetime::from_unix(1111111109);
    println("{}", crypto::totp::generate("12345678901234567890", at, {.digits = 8}));
}
```

Output:

```text
07081804
```

## See also

- [hotp](../hotp/README.md): the code of a counter
- [otp_key](../otp_key/README.md): a key as an app has it
- [The module](../README.md)
