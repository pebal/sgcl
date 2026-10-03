[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::from_modulus

```cpp
static expected<public_key, error> from_modulus(const slice<const byte>& n, uint64_t e) noexcept;
```

Makes a key of the modulus `n`, big-endian bytes with leading zeros allowed, and the public exponent `e`: the form a
JWK's `n` and `e` and a program's own format give a key in. The numbers are checked as every key's are: n odd, of 1024
to 16384 bits; e odd, 3 to 2³¹ − 1.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the modulus, big-endian |
| `e` | the public exponent |

## Return value

The key, or a [crypto::error](../error.md):

- `errc::invalid_key` for an even modulus, or an exponent that is even or below 3;
- `errc::unsupported` for a modulus of fewer than 1024 bits or more than 16384, or an exponent above 2³¹ − 1.

## Complexity

Quadratic in the bits of the modulus: the Montgomery constants are computed once, here.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    crypto::rsa::public_key pub = key.public_key();
    auto n = pub.modulus();
    auto same = crypto::rsa::public_key::from_modulus(n, 65537);
    println("{}", same == pub);
    println("{}", crypto::rsa::public_key::from_modulus(n, 4).error().message());
    auto half = n.as_slice().subslice(0, 64);
    println("{}", crypto::rsa::public_key::from_modulus(half, 65537).error().message());
}
```

Output:

```text
true
sgcl::crypto::rsa: the public exponent is not an odd number of at least 3
sgcl::crypto::rsa: a modulus of fewer than 1024 bits
```

## See also

- [modulus](modulus.md), [exponent](exponent.md): the numbers of a key
- [from_pkix_der](from_pkix_der.md): a key from its encoding
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
