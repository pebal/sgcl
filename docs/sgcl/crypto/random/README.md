[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::random

```cpp
#include "sgcl/crypto/random.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::random {
    void fill(const slice<byte>& out) noexcept;
    vector<byte> bytes(size_t n);
    secret_bytes secret(size_t n) noexcept;
}
```

`sgcl::crypto::random` is random bytes for keys, nonces, salts, session identifiers and tokens, Go's `crypto/rand`.
A ChaCha20 generator in user space, one per thread, gives them, so a key, a nonce, an ML-KEM encapsulation or an
ECDSA signature does not pay a trip to the kernel for its bytes. It is seeded from the operating system's
generator: `getentropy()` on macOS and on Linux (glibc 2.25, musl 1.1.20; it reads `getrandom()`, which waits once
at boot until the kernel's pool is seeded and never after), and `BCryptGenRandom` on Windows.

A request of up to 1 KiB is served from a buffer of the thread's generator; a larger one runs ChaCha20 under a key of
its own, at the speed of the cipher. [fill](fill.md) writes into any buffer, [bytes](bytes.md) returns
a new `vector<byte>` for what is not a secret, and [secret](secret.md) a [secret_bytes](../secret_bytes/README.md) for
what is. The costs are on [Benchmarks](../benchmarks.md#random).

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Fast key erasure.** A refill runs ChaCha20 under the thread's key for 576 bytes. The first 32 become the next key
  at once, and the other 544 are given out, each byte zeroed in the buffer as it leaves. A state read later, from a
  core dump or from a thread's memory after an exploit, tells nothing of what was given before. A request over 1 KiB
  takes a one-time key of its own and gets its bytes from ChaCha20 under that key directly.
- **Reseeded from the system** after 1 MiB given or 60 s since the last seed, both checked at a refill. The fresh
  bytes are mixed into the key, not put in its place, so that a weak system generator cannot spoil a state that is
  already good. The clock counts time asleep, so a machine woken after a minute reseeds at its next refill.
- **A forked child starts a stream of its own.** Its copy of the parent's state is thrown away and seeded afresh
  from the system before it gives a byte. A `pthread_atfork` handler, checked on every call, catches a fork; the
  process id, checked at every refill, catches a fork that ran no handlers.
- **A virtual machine cloned, or restored from a snapshot,** starts with the state the snapshot held, as in Go. The
  protection is the reseed every 1 MiB or 60 s and the fork counter: a clone's streams part at their next reseed, not
  at once. A program that makes long-lived keys right after a restore can wait for a fresh seed. There is no
  immediate guarantee.
- **The state lives in the thread's own storage**, never in managed memory. It holds the key, the buffer and a few
  counters, under 1 KiB, and is zeroed when the thread ends. A call after that, from the destructor of another
  `thread_local`, takes its bytes from the system directly.
- **There is no error to handle.** A system that cannot give random bytes cannot make a key safely, and a program
  that went on with zeros or with a weaker source would be worse off than one that stops. So a failure writes a line
  to stderr and calls `std::terminate`, as Go's `crypto/rand` has panicked since Go 1.24. On a working system it does
  not happen.
- **Not async-signal-safe**, as neither Go's nor OpenSSL's generator is. A signal handler that asks for random bytes
  while its thread is inside `fill` would take them from the same place.
- **For keys, not for simulations.** It is fast, but a simulation or a game wants a generator it can seed and replay,
  which [math::random](../../math/random/README.md) is.
- **Where the bytes go.** `fill` writes into any buffer (a stack array, a key's own storage, a slice of a vector)
  with no allocation. `bytes` returns a managed buffer: for what is not a secret (a salt, a nonce, a token that is
  public anyway). A key comes from `secret`: up to 64 bytes in the object itself, never in managed memory, zeroed
  when it goes.

## Member functions

| Function | Description |
|---|---|
| [fill](fill.md) | fills a buffer with random bytes |
| [bytes](bytes.md) | random bytes in a new `vector<byte>`, for what is not a secret |
| [secret](secret.md) | random bytes in a `secret_bytes`, for a key or a seed |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto token = crypto::random::bytes(16);  // public: sent to the client
    println("{}", encoding::hex::encode(token));

    crypto::secret_bytes key = crypto::random::secret(32);  // never printed
    crypto::chacha20_poly1305 aead(key);
    auto nonce = crypto::random::bytes(12);
    println("{}", aead.seal(nonce, token).size());
}
```

Sample output:

```text
d56221d29cbe6b849a50338f3dc400e7
32
```

## See also

- [secret_bytes](../secret_bytes/README.md): the form of a random key
- [secure_zero](../secure_zero.md): zeros over the program's own buffer of a key
- [hkdf](../hkdf/README.md), [pbkdf2](../pbkdf2/README.md): take a random salt
- [math::random](../../math/random/README.md): a generator to seed and replay
