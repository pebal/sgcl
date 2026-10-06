[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::decrypt

```cpp
static expected<vector<byte>, error> decrypt(const string& compact, const jwk& key) noexcept;         // (1)
static expected<vector<byte>, error> decrypt(const string& compact, const jwk_set& keys) noexcept;    // (2)
expected<vector<byte>, error> decrypt(const jwk& key) const noexcept;                                 // (3)
```

The plaintext of a JWE: its content key found by the algorithm of its header under the key, then the content
decrypted and its tag, which covers the header, checked.

1. Read the text first, as [parse](parse.md) reads it: the one line.
2. The same under the key of the header's `kid`, or each key of the set in turn when it names none.
3. This JWE, read before.

## Parameters

| Parameter | Description |
|---|---|
| `compact` | the JWE |
| `key` | the recipient's private key, or the shared oct key |
| `keys` | the recipient's keys |

## Return value

The plaintext, or an error: what [parse](parse.md) refuses; `errc::unsupported` for an `alg` or an `enc` the module
does not have and for `zip`; `errc::invalid_key` for a key that may not decrypt with the algorithm (its kind, its
`alg`, `use` or `key_ops`, a `dir` key of the wrong length, an `epk` not on the key's curve); `errc::authentication`
for a tag that does not match, which a key that does not unwrap the content key gives too.

## Complexity

Linear in the length of the JWE, and the key's management.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::a256gcmkw);
    auto token = crypto::jose::jwe::encrypt("for your eyes", key);
    println("{}", string(crypto::jose::jwe::decrypt(token, key).value()));
    auto other = crypto::jose::jwk::generate(crypto::jose::algorithm::a256gcmkw);
    println("{}", crypto::jose::jwe::decrypt(token, other).error().message());
}
```

Output:

```text
for your eyes
sgcl::crypto::jose: JWE: the content does not authenticate
```

## See also

- [encrypt](encrypt.md)
- [sgcl::crypto::jose::jwe](README.md)
