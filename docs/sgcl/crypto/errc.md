[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::errc

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    enum class errc : uint8_t {
        authentication = 1,
        invalid_key,
        invalid_signature,
        malformed,
        unsupported,
        verification
    };
}

template<>
struct std::is_error_code_enum<sgcl::crypto::errc> : std::true_type {};
```

What went wrong in data the module was given, the [code](error/code.md) of an [error](error.md): one list for the
whole module, as compress and encoding have one each. The values start at 1, since an `error_code` of 0 is success.
The specialization of `std::is_error_code_enum` makes a code convert to a `std::error_code` of the category
[crypto_category](crypto_category.md) by itself, through [make_error_code](make_error_code.md), for code that speaks
in error codes.

| Value | Description |
|---|---|
| `authentication` | an AEAD's tag does not match: the ciphertext, the associated data, the nonce or the key is not the sender's |
| `invalid_key` | a key from data that cannot be one: a point off the curve, a zero shared secret, a wrong size |
| `invalid_signature` | a signature that cannot be one: out of range, badly encoded |
| `malformed` | DER, ASN.1 or PEM that cannot be read |
| `unsupported` | an algorithm, a curve or a parameter the module does not do, an encrypted private key |
| `verification` | a certificate chain that does not verify; the error's [reason](error/reason.md) says why |

A broken contract of the program — a key of the wrong length given by the program, a nonce of the wrong size, more
output than an algorithm can give — is not here: it is `std::invalid_argument`, thrown.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <system_error>

using namespace sgcl;

int main() {
    auto key = crypto::x25519::public_key::from_bytes(encoding::hex::decode("0900"));
    println("{}", key.error().code() == crypto::errc::invalid_key);

    std::error_code code = crypto::errc::malformed;
    println("{} {}: {}", code.category().name(), code.value(), code.message());
}
```

Output:

```text
true
crypto 4: malformed data
```

## See also

- [error](error.md): the error that holds the code
- [make_error_code](make_error_code.md), [crypto_category](crypto_category.md): the code as a `std::error_code`
- [x509::reason](x509-reason.md): why a chain does not verify
