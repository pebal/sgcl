# sgcl::crypto::nonce_counter

```cpp
#include "sgcl/crypto/nonce_counter.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class nonce_counter;   // 96-bit nonces that never repeat: a counter, big-endian
}
```

**The implementation has not been through an independent cryptographic audit.**

The nonces of one key, never twice the same: a 96-bit counter whose `next()` is the one after the last, written big-endian in 12 bytes — the nonce [`aes_gcm`](aes_gcm.md) and [`chacha20_poly1305`](chacha20_poly1305.md) take. A counter cannot collide, where random 96-bit nonces collide with a probability that grows with the square of the number of messages; this is SP 800-38D §8.2.1's deterministic construction, and what TLS does with its record numbers. Go has no such type: a Go program keeps its own counter or uses `cipher.NewGCMWithRandomNonce`.

## Members

```cpp
static constexpr size_t nonce_size = 12;

nonce_counter() noexcept;                                   // from zero
explicit nonce_counter(const array<byte, 12>& start) noexcept;   // from a given nonce on

nonce_counter(nonce_counter&&) noexcept;                    // move-only; the counter moved from is spent
nonce_counter& operator=(nonce_counter&&) noexcept;

array<byte, 12> next();                                     // the next nonce; std::out_of_range when spent
```

## Rules

- **One counter per key, for as long as the key lives.** A counter started again from zero under the same key repeats every nonce: a program that restarts saves the last nonce it used and resumes from the one after it (`nonce_counter(start)`), or makes a new key.
- **Two senders of one key** (the two directions of a connection) need nonces that never meet: give each its own range — a different first byte of `start`, for example — or, better, a key each.
- **Move-only, and a counter moved from is spent**: a copy would hand out the same nonces twice, which is the one thing the type is for preventing. `next()` on a counter moved from, and after all 2^96 nonces, is `std::out_of_range`: the key must be replaced.
- A counter is not a secret and holds nothing to zero. It is not synchronized: one thread calls `next()` at a time, or each thread has its own range.

## Example

```cpp
#include "sgcl/crypto/nonce_counter.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    crypto::nonce_counter nonces;
    nonces.next();
    println(encoding::hex::encode(nonces.next()));
}
```

Output:

```text
000000000000000000000001
```

See [`aes_gcm`](aes_gcm.md#example) for the counter with a cipher.

## See also

[The module](README.md); [`aes_gcm`](aes_gcm.md); [`chacha20_poly1305`, `xchacha20_poly1305`](chacha20_poly1305.md) — the latter takes random nonces safely.
