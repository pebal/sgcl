[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::from_sec1_der

```cpp
static expected<private_key, error> from_sec1_der(const slice<const byte>& der) noexcept;
```

Reads a SEC 1 ECPrivateKey (SEC 1 §C.4, RFC 5915) of this curve, Go's `x509.ParseECPrivateKey`: the DER of an
`EC PRIVATE KEY` block in PEM, the form OpenSSL's `ecparam -genkey` writes. Version 1, the scalar, and when they are
there the parameters, which must name this curve, and the public key, which is passed over. The DER is read strictly;
a scalar with a zero byte too many or too few is read as Go and OpenSSL read it.

`p384::private_key::from_sec1_der` reads the keys of P-384.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the DER of the ECPrivateKey |

## Return value

The key, or a [crypto::error](../error/README.md) with the offset of the byte where the reading stopped:

- `errc::malformed` for DER that is not an ECPrivateKey;
- `errc::unsupported` for parameters that name another curve;
- `errc::invalid_key` for a scalar not in [1, n − 1] or longer than the curve's order.

## Complexity

Linear in the size of `der`, and one multiplication of the base point.

## Exceptions

None.

## Notes

The DER holds the secret scalar: it belongs in a [secret_bytes](../secret_bytes/README.md), as
[to_sec1_der](to_sec1_der.md) gives it, never in managed memory.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    auto stored = key.to_sec1_der();

    auto read = crypto::p256::private_key::from_sec1_der(stored);
    println("{}", read->public_key() == key.public_key());

    // the parameters of a P-384 key name another curve
    auto other = crypto::p384::private_key::generate().to_sec1_der();
    auto refused = crypto::p256::private_key::from_sec1_der(other);
    println("{}", refused.error().message());
    println("{}", refused.error().code() == crypto::errc::unsupported);
}
```

Output:

```text
true
offset 58: sgcl::crypto::p256: the key's parameters are not this curve's
true
```

## See also

- [to_sec1_der](to_sec1_der.md): the ECPrivateKey of a key
- [from_pkcs8_der](from_pkcs8_der.md): the form `PRIVATE KEY`
- [from_pem](from_pem.md): either form in PEM
- [sgcl::crypto::p256::private_key](README.md)
