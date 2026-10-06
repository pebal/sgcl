[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::sign

```cpp
vector<byte> sign(const slice<const byte>& message) const;
vector<byte> sign(const slice<const byte>& message, const options& o) const;
```

The signature of `message` (ML-DSA.Sign, FIPS 204 Algorithm 2) under the context string of `o`: hedged by default, its signing randomness 32 bytes of [crypto::random](../random/README.md) mixed with the key and the message, so that two signatures of one message differ and a fault in the randomness does not give the key away; deterministic with `o.deterministic`, the randomness all zeros (Go's `SignDeterministic`), the same signature every time. Both verify the same way.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message: bytes or text, of any length |
| `o` | the context and the determinism ([options](../mldsa65-options.md)) |

## Return value

The signature, `signature_size` bytes (2420, 3309, 4627 for the three sets).

## Complexity

Linear in the length of the message; the signing loop runs 5.1 rounds on average for ML-DSA-65 (4.25 for 44, 3.85 for 87), each its own rejection by FIPS 204's design.

## Exceptions

- `std::invalid_argument` for a context over 255 bytes.
- `std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a key of a fixed seed (a program makes its own with generate())
    auto key = crypto::mldsa65::private_key::from_seed(
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")).value();
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

- [public_key::verify](../mldsa65-public_key/verify.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
