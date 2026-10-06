[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::otp_type

```cpp
#include "sgcl/crypto/otp.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    enum class otp_type : uint8_t {
        hotp,
        totp
    };
}
```

Which kind of one-time password an [otp_key](otp_key/README.md) makes: the type of its otpauth:// URI.

| Value | Description |
|---|---|
| `hotp` | a code of a counter (RFC 4226), `otpauth://hotp/` |
| `totp` | a code of the time (RFC 6238), `otpauth://totp/`; the default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key("otpauth://hotp/alice?secret=JBSWY3DPEHPK3PXP&counter=7");
    println("{}", key.type == crypto::otp_type::hotp);
}
```

Output:

```text
true
```

## See also

- [otp_key](otp_key/README.md): the key that has a type
- [The module](README.md)
