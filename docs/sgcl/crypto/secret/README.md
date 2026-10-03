[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::secret\<N\>

```cpp
#include "sgcl/crypto/secret.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    template<size_t N>
    class secret;
}
```

`sgcl::crypto::secret<N>` is `N` bytes that are a secret, as the module gives them out: an ECDH shared secret, a
private key's scalar, a seed. Every accessor of private material of a fixed length returns one: the shared secrets of
[x25519](../x25519.md), [p256](../p256.md) and [p384](../p384.md) (`secret<32>`, `secret<32>`, `secret<48>`), a private key's
`bytes()` (its scalar, or X25519's 32 bytes), [ed25519](../ed25519.md)'s `seed()` (`secret<32>`) and `bytes()`
(`secret<64>`). Public keys stay `array<byte, N>`. The bytes live in the object itself, with no allocation, so that
they have one place, and that place is cleared when the object goes.

A program does not make one: there is no public constructor but the move. A secret whose length is known only when
the program runs, what a key derivation gives or a file holds, is a [secret_bytes](../secret_bytes/README.md). Go has neither:
its keys and secrets are `[]byte`, left to the garbage collector.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Move-only.** A copy is asked for by name, with [clone](clone.md). A move leaves the source zeroed, and the
  destructor zeroes the bytes with stores the compiler cannot drop ([secure_zero](../secure_zero.md)).
- **Read in place.** [bytes](bytes.md), and the conversion to `slice<const byte>`, give a slice without an
  owner over the object's own bytes, valid while the object lives. Every function of the module that takes bytes
  takes it: `hkdf_sha256::derive(salt, shared, info, 32)`, `hmac_sha256(key)`. Two secrets are compared with `==`,
  which is [constant_time::equal](../constant_time/equal.md), never with a loop that stops at the first difference.
- **Where it lives.** On the stack or in a `unique_ptr`, a secret is gone when its scope ends. In a managed object it
  stays in memory until the cycle that finds the object dead, and after it: managed memory is not zeroed when an
  object dies, and a block the collector frees keeps its bytes until it is given out again. So a secret belongs on
  the stack or in plain memory. Copies made of its bytes into buffers of the program's own are the program's to
  clear.

## Template parameters

| Parameter | Description |
|---|---|
| `N` | the number of bytes |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `size` | `N` | the number of bytes, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](secret.md) | takes the bytes of another secret over and zeroes them there |
| `(destructor)` | zeroes the bytes |
| [operator=](operator_assign.md) | takes the bytes of another secret over and zeroes them there |
| [clone](clone.md) | a second secret of the same bytes |

#### Element access

| Function | Description |
|---|---|
| [bytes, operator slice\<const byte\>](bytes.md) | the bytes, as a slice over the object's own memory |

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
    // RFC 7748 §6.1's keys; a program makes its own with generate()
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    auto bob = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"));

    crypto::secret<32> shared = alice->shared_secret(bob->public_key()).value();
    println("{} {}", shared.size, shared == bob->shared_secret(alice->public_key()).value());

    // the secret goes through a key derivation, read in place
    auto key = crypto::hkdf_sha256::derive("salt", shared, "chat v1 key", 32);
    println("{}", key.size());
}
```

Output:

```text
32 true
32
```

## See also

- [secret_bytes](../secret_bytes/README.md): a secret of a length known when the program runs
- [secure_zero](../secure_zero.md): zeros the compiler cannot drop, for the program's own buffers
- [constant_time](../constant_time/README.md): the comparison of secrets
