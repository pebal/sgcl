[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::from_bytes

```cpp
static expected<private_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

The key of its 4n bytes, SK.seed ‖ SK.prf ‖ PK.seed ‖ PK.root (FIPS 205 §9.1), as [bytes](bytes.md) gives them and OpenSSL's raw private key holds them. PK.root is computed of the seeds again and compared, in constant time, with the one given: the key cannot sign under a public key that is not its own.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | `private_key_size` bytes (64 for the 128 sets, 96, 128): a [secret_bytes](../secret_bytes/README.md) of [read_secret](../read_secret.md), bytes |

## Return value

The key, or `errc::invalid_key` for another length or a PK.root that is not the seeds'.

## Complexity

Constant: one XMSS tree, as [generate](generate.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto again = crypto::slhdsa_sha2_128s::private_key::from_bytes(key.bytes());
    println("{}", *again == key);
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
