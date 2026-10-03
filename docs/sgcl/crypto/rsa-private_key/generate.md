[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::generate

```cpp
static private_key generate(size_t bits);
```

Makes a new key of `bits` bits, its randomness from [crypto::random](../random/README.md): two primes of half the bits
each (fresh random candidates with their top two bits set, trial division by the primes below 2048, sixteen rounds
of Miller–Rabin), e = 65537, |p − q| above 2^(bits/2 − 100) as FIPS 186-5 asks, and d = e⁻¹ mod λ(n) = lcm(p − 1,
q − 1), the smallest d, as FIPS 186-5 (B.3.1) and SP 800-56B ask, so that the keys pass a FIPS validator (OpenSSL's
FIPS provider, an HSM) as well as any other. The gcd of p − 1 and q − 1 and the quotient that gives λ are computed in
constant time. The key is checked whole, as one read is, before it is given out.

## Parameters

| Parameter | Description |
|---|---|
| `bits` | the bits of the modulus, 2048 to 16384 |

## Return value

The new key.

## Complexity

A candidate's test costs the cube of its length and the primes are sparser as they grow, so the time grows quickly
with `bits`: tens of milliseconds for 2048 bits, more for each step up. It varies from key to key as the primes are
found.

## Exceptions

`invalid_argument` when `bits` is below 2048 or above 16384.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::rsa::private_key::generate(3072);
    println("{} bits, e = {}", key.bits(), key.public_key().exponent());
    try {
        crypto::rsa::private_key::generate(1024);
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
3072 bits, e = 65537
sgcl::crypto::rsa: a key of 1024 bits (2048 to 16384 are made)
```

## See also

- [from_pem](from_pem.md): a key that was made before
- [to_pem](to_pem.md): the key written to be kept
- [sgcl::crypto::rsa::private_key](README.md)
