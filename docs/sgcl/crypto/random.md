# sgcl::crypto::random

```cpp
#include "sgcl/crypto/random.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto::random {
    void fill(const slice<byte>& out) noexcept;   // out filled with random bytes
    vector<byte> bytes(size_t n);                 // n random bytes: a salt, a nonce, an id
    secret_bytes secret(size_t n);                // n random bytes that are a secret: a key, a seed
}
```

Random bytes for keys, nonces, salts, session identifiers and tokens, Go's `crypto/rand`. A ChaCha20 generator in user space, one per thread, gives them. It is seeded from the operating system's generator: `getentropy()` on macOS and Linux (glibc 2.25, musl 1.1.20; it reads `getrandom()`, which waits once at boot until the kernel's pool is seeded and never after), and `BCryptGenRandom` on Windows. A request of 32 bytes costs about 21 ns, where a system call cost 1.5 µs and OpenSSL's `RAND_bytes` takes 200 ns, so a key, a nonce, an ML-KEM encapsulation or an ECDSA signature no longer pays a trip to the kernel. Large requests run at the speed of ChaCha20, about 2.4 GB/s on one core.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Fast key erasure.** A refill runs ChaCha20 under the thread's key for 576 bytes. The first 32 become the next key at once, and the other 544 are given out, each byte zeroed in the buffer as it leaves. A state read later, from a core dump or from a thread's memory after an exploit, tells nothing of what was given before. A request over 1 KiB takes a one-time key of its own and gets its bytes from ChaCha20 under that key directly.
- **Reseeded from the system** after 1 MiB given or 60 s since the last seed, both checked at a refill. The fresh bytes are mixed into the key, not put in its place, so that a weak system generator cannot spoil a state that is already good. The clock counts time asleep, so a machine woken after a minute reseeds at its next refill.
- **A forked child starts a stream of its own.** Its copy of the parent's state is thrown away and seeded afresh from the system before it gives a byte. A `pthread_atfork` handler, checked on every call, catches a fork; the process id, checked at every refill, catches a fork that ran no handlers.
- **A virtual machine cloned, or restored from a snapshot,** starts with the state the snapshot held, as in Go. The protection is the reseed every 1 MiB or 60 s and the fork counter: a clone's streams part at their next reseed, not at once. A program that makes long-lived keys right after a restore can wait for a fresh seed. There is no immediate guarantee.
- **The state lives in the thread's own storage**, never in managed memory. It holds the key, the buffer and a few counters, under 1 KiB, and is zeroed when the thread ends. A call after that, from the destructor of another `thread_local`, takes its bytes from the system directly.
- **There is no error to handle.** A system that cannot give random bytes cannot make a key safely, and a program that went on with zeros or with a weaker source would be worse off than one that stops. So a failure writes a line to stderr and calls `std::terminate`, as Go's `crypto/rand` has panicked since Go 1.24. On a working system it does not happen.
- **Not async-signal-safe**, as neither Go's nor OpenSSL's generator is. A signal handler that asks for random bytes while its thread is inside `fill` would take them from the same place.
- **For keys, not for simulations.** It is fast, but a simulation or a game wants a generator it can seed and replay, which [`math::random`](../math/random.md) is.
- **`fill` writes into any buffer** (a stack array, a key's own storage, a slice of a vector) with no allocation. `bytes` returns a new `vector<byte>`, a managed buffer: for what is not a secret (a salt, a nonce, a token that is public anyway). A key comes from `secret`, a [`secret_bytes`](secret.md#secret_bytes): up to 64 bytes in the object itself, never in managed memory, zeroed when it goes.

## Members

### fill

```cpp
void fill(const slice<byte>& out) noexcept;
```

Fills `out` with random bytes, with no allocation: a stack array, a key's own storage, a slice of a vector.

### bytes

```cpp
vector<byte> bytes(size_t n);
```

`n` random bytes in a new `vector<byte>`, a managed buffer: for what is not a secret (a salt, a nonce, an id, a token that is public anyway).

### secret

```cpp
secret_bytes secret(size_t n);
```

`n` random bytes that are a secret (a key, a seed) in a [`secret_bytes`](secret.md#secret_bytes): up to 64 bytes in the object itself, never in managed memory, zeroed when it goes.

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

#include <array>

using namespace sgcl;

int main() {
    auto token = crypto::random::bytes(16);
    println(encoding::hex::encode(token));

    std::array<byte, 32> key;  // on the stack
    crypto::random::fill(key);
    // ... use the key ...
    crypto::secure_zero(key);
}
```

Sample output:

```text
3cc54073e13fa0fdeba73a064ca0243b
```

## See also

[`secure_zero`](secure_zero.md); [`hkdf`](hkdf.md), [`pbkdf2`](pbkdf2.md), which take a random salt.
