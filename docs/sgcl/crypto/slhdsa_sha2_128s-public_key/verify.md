[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [public_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::public_key::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept;
[[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature,
                          const options& o) const noexcept;
```

Whether `signature` is the key's signature of `message` under the context of `o` (slh_verify, FIPS 205 Algorithm 24), hedged or deterministic alike. A signature of another length and a context over 255 bytes are `false`, as is any that does not verify.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message: bytes or text |
| `signature` | the signature |
| `o` | the context the signature was made under ([options](../slhdsa_sha2_128s-options.md)); `deterministic` is not read |

## Return value

`true` when the signature verifies.

## Complexity

Linear in the length of the message, besides the scheme's hashes: a FORS public key and d XMSS roots from their paths (under a tenth of a millisecond for SHA2-128s, a quarter for its fast set).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto pub = key.public_key();
    auto sig = key.sign("hello");
    println("{} {}", pub.verify("hello", sig), pub.verify("hellO", sig));
}
```

Output:

```text
true false
```

## See also

- [private_key::sign](../slhdsa_sha2_128s-private_key/sign.md)
- [sgcl::crypto::slhdsa_sha2_128s::public_key](README.md)
