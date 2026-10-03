[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::secret_bytes

```cpp
#include "sgcl/crypto/secret.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class secret_bytes;
}
```

`sgcl::crypto::secret_bytes` is bytes that are a secret, of a length known only when the program runs: what
[hkdf](../hkdf/README.md) and [pbkdf2](../pbkdf2/README.md) derive, SHAKE's output ([shake256](../shake256/README.md)), a key from
[random::secret](../random/secret.md), a private key's export (`to_pkcs8_der`, `to_pem`), a key file read by
[read_secret](../read_secret.md). It is the module's rule that a secret is never in managed memory, and this is the
form a secret of any length takes: never a managed `vector<byte>`, which the collector frees without zeroing.
[secret\<N\>](../secret/README.md) is the one for a length known when the program is compiled. Go has neither: its keys are
`[]byte`.

A plaintext is not a secret of this kind: what an AEAD opens and what [rsa](../rsa.md) decrypts is the user's data and
comes back as a `vector<byte>`. Their `_to` forms write into a buffer of the program's, a `secret_bytes` among them,
for a key unwrapped.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Where the bytes are.** Up to 64 bytes live in the object itself, so keys of 32, 48 or 64 bytes, derived keys
  and shared secrets allocate nothing. Past that they live in a block of plain memory (`::operator new`), never
  managed, zeroed before it is freed. A growth zeroes the block, or the inline bytes, it leaves.
- **Move-only.** A copy is asked for by name, with [clone](clone.md); a move leaves the source empty,
  its bytes zeroed. The destructor zeroes what the object held, with stores the compiler cannot drop
  ([secure_zero](../secure_zero.md)).
- **Read and written through [as_slice](as_slice.md).** It gives a slice without an owner, which every
  function of the module that takes bytes takes; the conversions make that implicit, to `slice<const byte>` where
  bytes are read and, for a `secret_bytes` the program may change, to `slice<byte>` where the module writes them
  (`random::fill`, an AEAD's `open_to`, rsa's `decrypt_oaep_to`). There is no `push_back` and no `operator[]`: a
  secret is not a container.
- **Where it lives.** On the stack or in a `unique_ptr`, its bytes are gone when its scope ends. Inside a managed
  object its inline bytes would lie in managed memory until that object is collected, and after it, since managed
  memory is not zeroed when an object dies; so keep it out of one.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `inline_capacity` | `64` | the bytes held in the object itself, with no allocation, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](secret_bytes.md) | constructs an empty secret, `n` zero bytes, or takes another's bytes over |
| `(destructor)` | zeroes the bytes and frees their block |
| [operator=](operator_assign.md) | takes the bytes of another secret over |
| [clone](clone.md) | a second secret of the same bytes |

#### Element access

| Function | Description |
|---|---|
| [as_slice, operator slice\<const byte\>, operator slice\<byte\>](as_slice.md) | the bytes, as a slice over the object's own memory |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of bytes |
| [empty](empty.md) | checks whether there are no bytes |
| [resize](resize.md) | changes the number of bytes, the new ones zero |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two secrets in constant time |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869's test case 1
    auto ikm = encoding::hex::decode("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    auto info = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9");
    crypto::secret_bytes okm = crypto::hkdf_sha256::derive(salt, ikm, info, 42);

    auto published = encoding::hex::decode("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db0"
                                           "2d56ecc4c5bf34007208d5b887185865");
    println("{} {}", okm.size(), crypto::constant_time::equal(okm, published));

    crypto::secret_bytes key = crypto::random::secret(32);  // for a cipher
    crypto::chacha20_poly1305 aead(key);
    println("{}", aead.seal(encoding::hex::decode("000000000000000000000001"), "hello").size());
}
```

Output:

```text
42 true
21
```

## See also

- [secret\<N\>](../secret/README.md): a secret of a length known when the program is compiled
- [read_secret](../read_secret.md): a file's bytes as a secret
- [random::secret](../random/secret.md): random bytes as a secret
- [secure_zero](../secure_zero.md): zeros the compiler cannot drop, for the program's own buffers
