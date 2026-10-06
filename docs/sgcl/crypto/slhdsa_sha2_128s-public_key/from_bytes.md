[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [public_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

The public key of its 2n bytes, PK.seed ‖ PK.root (FIPS 205 §9.1), as [bytes](bytes.md) gives them and OpenSSL's raw public key holds them.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | `public_key_size` bytes (32 for the 128 sets, 48, 64) |

## Return value

The key, or `errc::invalid_key` for another length. Every string of the right length is a key.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto read = crypto::slhdsa_sha2_128s::public_key::from_bytes(key.public_key().bytes());
    println("{}", read->verify("m", key.sign("m")));
    println("{}", bool(crypto::slhdsa_sha2_128s::public_key::from_bytes(vector<byte>(10))));
}
```

Output:

```text
true
false
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::public_key](README.md)
