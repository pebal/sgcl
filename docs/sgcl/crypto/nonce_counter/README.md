[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::nonce_counter

```cpp
#include "sgcl/crypto/nonce_counter.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class nonce_counter;
}
```

`sgcl::crypto::nonce_counter` gives the nonces of one key, never twice the same: a 96-bit counter whose
[next](next.md) is the one after the last, written big-endian in 12 bytes, the nonce
[aes_gcm](../aes_gcm/README.md) and [chacha20_poly1305](../chacha20_poly1305/README.md) take. A counter cannot collide, where random
96-bit nonces collide with a probability that grows with the square of the number of messages: this is SP 800-38D
§8.2.1's deterministic construction, and what TLS does with its record numbers.

Go has no such type: a Go program keeps its own counter, or uses `cipher.NewGCMWithRandomNonce`, whose place here
is [xchacha20_poly1305](../xchacha20_poly1305/README.md)'s `seal_random`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **One counter per key, for as long as the key lives.** A counter started again from zero under the same key
  repeats every nonce: a program that restarts saves the last nonce it used and resumes from the one after it
  (`nonce_counter(start)`), or makes a new key.
- **Two senders of one key** (the two directions of a connection) need nonces that never meet: give each its own
  range, a different first byte of `start` for example, or, better, a key each.
- **Move-only, and a counter moved from is spent**: a copy would hand out the same nonces twice, which is the one
  thing the type is there to prevent. `next()` on a counter moved from, and after all 2^96 nonces, is
  `out_of_range`: the key must be replaced.
- **Not a secret and not synchronized.** A counter holds nothing to zero. One thread calls `next()` at a time, or
  each thread has its own counter over a range of its own.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `nonce_size` | `12` | the bytes of a nonce, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](nonce_counter.md) | a counter from zero, from a given nonce on, or another's taken over |
| [operator=](operator_assign.md) | takes another counter over |
| [next](next.md) | the next nonce |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::nonce_counter nonces;
    nonces.next();
    println("{}", encoding::hex::encode(nonces.next()));

    // The second sender of the same key, in a range of its own
    array<byte, 12> start = {};
    start[0] = byte(0x80);
    crypto::nonce_counter replies(start);
    println("{}", encoding::hex::encode(replies.next()));
}
```

Output:

```text
000000000000000000000001
800000000000000000000000
```

## See also

- [aes_gcm](../aes_gcm/README.md), [chacha20_poly1305](../chacha20_poly1305/README.md): the AEADs that take its nonces
- [xchacha20_poly1305](../xchacha20_poly1305/README.md): random nonces, where a counter cannot be kept
- [The module](../README.md)
