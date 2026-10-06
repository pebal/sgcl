[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [public_key](README.md)

# sgcl::crypto::mldsa65::public_key::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept;
[[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature,
                          const options& o) const noexcept;
```

Whether `signature` is the key's signature of `message` under the context of `o` (ML-DSA.Verify, FIPS 204 Algorithm 3), hedged or deterministic alike. A signature of another length, a context over 255 bytes, hints that do not decode (indices not increasing, counts past ω, padding not zero) and a response over its bound are `false`, as is any that does not verify.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message: bytes or text |
| `signature` | the signature |
| `o` | the context the signature was made under ([options](../mldsa65-options.md)); `deterministic` is not read |

## Return value

`true` when the signature verifies.

## Complexity

Linear in the length of the message; the matrix is the key's, made once.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
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

- [private_key::sign](../mldsa65-private_key/sign.md)
- [sgcl::crypto::mldsa65::public_key](README.md)
