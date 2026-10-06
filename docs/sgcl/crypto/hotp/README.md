[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::hotp

```cpp
#include "sgcl/crypto/otp.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class hotp;
}
```

`sgcl::crypto::hotp` is HOTP of RFC 4226: a one-time password of six to ten digits from an HMAC of a counter under a
secret both sides share, dynamically truncated to 31 bits and taken modulo a power of ten. Each side counts: a token
generates the code of its next counter, and the server [verify](verify.md)s it and moves its own counter past the one
that matched. [totp](../totp/README.md) is the same with the time as the counter, what most apps use; HOTP is the
hardware tokens' and some banks'.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The secret** is at least 16 bytes and better 20 (RFC 4226's 160 bits): [random::secret](../random/secret.md)(20),
  kept as a key is kept. An [otp_key](../otp_key/README.md) holds it with its options and counter.
- **A check looks ahead** [otp_options](../otp_options.md)' `skew` counters past the expected one, for codes a token
  generated and nobody used, and never behind: a code once accepted is not accepted again once the server has moved
  its counter past the match.
- **In constant time**: every code of the window is computed and compared with the one given, with no early exit; a
  code of the wrong length or with other characters than digits is refused at once, its format being public.
- **Options out of range** are `std::invalid_argument`.

## Member functions

| Function | Description |
|---|---|
| [generate](generate.md) | the code of a counter (static) |
| [verify](verify.md) | the counter whose code a given code is, in a window (static) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4226 Appendix D: the codes of counters 0 to 3
    for (int counter : range(4)) {
        println("{}", crypto::hotp::generate("12345678901234567890", counter));
    }
}
```

Output:

```text
755224
287082
359152
969429
```

## See also

- [totp](../totp/README.md): the code of the time
- [otp_key](../otp_key/README.md): a key as an app has it
- [The module](../README.md)
