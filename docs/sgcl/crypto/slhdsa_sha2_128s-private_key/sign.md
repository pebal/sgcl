[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::sign

```cpp
vector<byte> sign(const slice<const byte>& message) const;
vector<byte> sign(const slice<const byte>& message, const options& o) const;
```

The signature of `message` (slh_sign, FIPS 205 Algorithm 22) under the context string of `o`: hedged by default, its randomizer R made of n bytes of [crypto::random](../random/README.md) with SK.prf and the message, so that two signatures of one message differ; deterministic with `o.deterministic`, the randomness PK.seed (§10.2.1), the same signature every time. Both verify the same way.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message: bytes or text, of any length |
| `o` | the context and the determinism ([options](../slhdsa_sha2_128s-options.md)) |

## Return value

The signature, `signature_size` bytes (7856 for SLH-DSA-SHA2-128s; 17088 for its fast set, up to 49856 for SHAKE-256f).

## Complexity

Linear in the length of the message, besides the hashes of the scheme: a FORS signature and d XMSS trees of 2^h' leaves (90 ms for SHA2-128s, 5 ms for SHA2-128f).

## Exceptions

- `std::invalid_argument` for a context over 255 bytes.
- `std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto a = key.sign("hello", {.deterministic = true});
    auto b = key.sign("hello", {.deterministic = true});
    auto c = key.sign("hello");
    println("{} {}", a == b, a == c);
    println("{}", key.public_key().verify("hello", c));
}
```

Output:

```text
true false
true
```

## See also

- [public_key::verify](../slhdsa_sha2_128s-public_key/verify.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
