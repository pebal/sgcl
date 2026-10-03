[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](../ed25519-public_key.md)

# sgcl::crypto::ed25519::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Makes the key of 32 bytes, the canonical encoding of a point on the curve, as RFC 8032 §5.1.3 decodes it, and
decompresses the point once for every verification to come. An encoding of y = p or more (`ed…ff7f`, `ee…ff7f`) and
an x of 0 with the sign bit set (the "negative zero") are not points by the RFC and are refused, where Go's
`ed25519.Verify` and OpenSSL reduce them and accept the signatures under them. A point of small order is taken, as
the RFC and Go take it.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the 32 bytes of the key |

## Return value

The key, or an [error](../error.md) `errc::invalid_key` when `bytes` is not 32 bytes long or not the encoding of a
point of the curve.

## Complexity

Constant: one decompression, a square root in the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 key
    auto key = crypto::ed25519::public_key::from_bytes(
        encoding::hex::decode("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"));
    println("{}", key.has_value());

    // y = p, a value Go reduces to 0
    auto big_y = crypto::ed25519::public_key::from_bytes(
        encoding::hex::decode("edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f"));
    println("{}", big_y.error().message());

    auto short_key = crypto::ed25519::public_key::from_bytes(encoding::hex::decode("d75a98"));
    println("{}", short_key.error().message());
}
```

Output:

```text
true
not the encoding of a point of Ed25519
an Ed25519 public key is 32 bytes
```

## See also

- [from_pkix_der](from_pkix_der.md): the key of a SubjectPublicKeyInfo
- [bytes](bytes.md): the reverse
- [sgcl::crypto::ed25519::public_key](../ed25519-public_key.md)
