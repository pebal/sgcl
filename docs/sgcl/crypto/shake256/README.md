[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::shake256

```cpp
#include "sgcl/crypto/sha3.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class shake256;
    class shake128;
}
```

`sgcl::crypto::shake256` is SHAKE256 of FIPS 202, Go's `sha3.NewShake256`: an extendable-output function (XOF), the
Keccak-f[1600] sponge of the [SHA-3 digests](../sha3_256/README.md) with other domain bits, whose output has no fixed length.
It takes input as a digest does, and then gives as many bytes as asked, and more after them. `shake128` is SHAKE128,
the same sponge with a rate of 168 bytes where `shake256` has 136, and half the security; everything else is as for
`shake256`. SHAKE is what ML-KEM and ML-DSA are built on, and a general way to draw any number of bytes from a seed.
Written from FIPS 202 and tested against its examples and against OpenSSL.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Not a hasher**: its output has no fixed length, so it has no `value()` and is not a `hash::req::hasher`.
  [read](read.md) gives the next `n` bytes and [read_to](read_to.md) fills a buffer; the first read
  closes the input, and every read goes on where the last one stopped, so two reads of 16 bytes give what one read of
  32 gives. `update` after a read is a broken contract: `std::invalid_argument`. [reset](reset.md) opens the
  input again. [of](of.md) is the one-shot form.
- **The security** is the smaller of its strength (128 or 256 bits) and half the output's bits for collisions: read
  at least 32 bytes of SHAKE128, 64 of SHAKE256, where collisions matter.
- **The output is a secret** as often as not (SHAKE derives keys): `read` gives a
  [secret_bytes](../secret_bytes/README.md), up to 64 bytes in the object itself, past that in plain memory zeroed when it goes,
  never in managed memory; `read_to` writes into a buffer of the caller's.
- **A copy is a branch**, and a copy that is reading reads on alike. Each holds the 200 bytes of the state and a
  position, trivially copyable and nothing for the collector, so it lives anywhere a plain struct does. The state is
  not zeroed when it dies.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `block_size` | `136` (`168` for `shake128`) | the rate: the bytes of the state a block fills, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](shake256.md) | a sponge with its input open |
| `(destructor)` | trivial: the state is not zeroed |

#### Hashing

| Function | Description |
|---|---|
| [update](update.md) | absorbs bytes or text, until the first read |
| [read](read.md) | the next bytes of the output, as a `secret_bytes` |
| [read_to](read_to.md) | the next bytes of the output, into a buffer |
| [reset](reset.md) | as new: the input open again |
| [of](of.md) | the first bytes of the output over data, in one call (static) |

## Complexity

Linear in the bytes absorbed and the bytes read, one permutation a block of the rate; the permutation runs on the
SHA-3 instructions of ARMv8.2 where the processor has them, as for the [digests](../sha3_256/README.md#complexity).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // as many bytes as asked, and more after them
    crypto::shake256 x;
    x.update("seed");
    auto first = x.read(16);
    auto next = x.read(16);
    println(encoding::hex::encode(first));
    println(encoding::hex::encode(next));
    println(encoding::hex::encode(crypto::shake256::of("seed", 32)));
}
```

Output:

```text
4fd6800b5ddf65323de29f59e5da90d3
fa6778594e60e2ff4326622eff3e42c4
4fd6800b5ddf65323de29f59e5da90d3fa6778594e60e2ff4326622eff3e42c4
```

## See also

- [sha3_256](../sha3_256/README.md): the digests of the same sponge
- [hkdf](../hkdf/README.md): keys of any length from a secret, over a digest
- [secret_bytes](../secret_bytes/README.md): what `read` gives
- [The module](../README.md)
