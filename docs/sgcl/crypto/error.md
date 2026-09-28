# sgcl::crypto::error

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    enum class errc : uint8_t {
        authentication = 1, invalid_key, invalid_signature, malformed, unsupported, verification
    };
    class error;
    const std::error_category& crypto_category() noexcept;
}
```

The error of the whole module: what went wrong in data it was given, returned in an `expected` (`open` of an AEAD, a key or a certificate parsed from bytes). A value, copied, compared, held in an `expected`.

**The implementation has not been through an independent cryptographic audit.**

| code | what |
|---|---|
| `authentication` | an AEAD's tag does not match: the ciphertext, the associated data, the nonce or the key is not the sender's |
| `invalid_key` | a key from data that cannot be one: a point off the curve, a zero shared secret, a wrong size |
| `invalid_signature` | a signature that cannot be one: out of range, badly encoded |
| `malformed` | DER, ASN.1 or PEM that cannot be read |
| `unsupported` | an algorithm, a curve or a parameter the module does not do |
| `verification` | a certificate chain that does not verify: `reason()` says why ([`x509`](x509.md)) |

A broken contract is not an error of this list: a key of the wrong length given by the program, a nonce of the wrong size, more output than an algorithm gives (`hkdf::expand` past 255 blocks), `update` of a SHAKE after a read, a `hash_id` outside the list — each is `std::invalid_argument`, thrown, as in the hash and compress modules. No random bytes from the system is neither: [`random`](random.md) terminates.

## Members

```cpp
error();
explicit error(errc code);
error(errc code, const string& detail);                 // the detail said in place of the code's own words
error(errc code, uint64_t offset);                      // at a byte of an encoded input
error(errc code, uint64_t offset, const string& detail);
error(x509::reason why, const string& detail);         // errc::verification and why
errc code() const noexcept;
uint64_t offset() const noexcept;                       // 0 for data that is not an encoding
x509::reason reason() const noexcept;                   // why a chain does not verify; none for any other error
string message() const;                                 // "message authentication failed", "offset 17: malformed data"
friend bool operator==(const error&, const error&) noexcept;
```

`make_error_code(errc)` and `crypto_category()` make an `error_code` of the category `"crypto"`.

## Example

```cpp
#include "sgcl/crypto/error.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    println(crypto::error(crypto::errc::authentication).message());
    println(crypto::error(crypto::errc::malformed, 17).message());
}
```

Output:

```text
message authentication failed
offset 17: malformed data
```
