[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::argon2

```cpp
#include "sgcl/crypto/argon2.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class argon2;
}
```

`sgcl::crypto::argon2` is Argon2 of RFC 9106, Go's `golang.org/x/crypto/argon2`: the password hash that won the
Password Hashing Competition, made slow on purpose and hungry for memory, so that every guess of an attacker who has
a stored hash costs megabytes of memory as well as time, which a graphics card or a custom chip cannot make cheap the
way it makes a fast digest cheap. Its variant and its costs are an [options](../argon2-options.md):
[Argon2id](../argon2-variant.md) by default, with 64 MiB, three passes and four lanes, RFC 9106's second recommended
setting.

The class has two uses. [generate](generate.md) and [verify](verify.md) store passwords: `generate` hashes a password
with a random salt into a PHC string, `$argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>`, which holds everything a check
needs but the password, and `verify` takes the password and the string and says whether they match — the form Python's
`argon2-cffi`, PHP's `password_hash` and the reference implementation write and read. [derive](derive.md) and
[derive_to](derive_to.md) are the raw function, a key of any length from a password and a salt, as
[pbkdf2](../pbkdf2/README.md) is, where a format names Argon2 as its key derivation.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The costs are the program's.** The defaults are a floor for an interactive login on a server; RFC 9106's first
  recommendation is 2 GiB with one pass, for a machine that has the memory. A higher cost costs every login on the
  server as much as it costs an attacker's guess; [verify](verify.md) reads the costs from the string, so they may be
  raised for new hashes while old ones still verify.
- **The lanes run on the scheduler's workers** ([async::parallel_for](../../async/parallel_for.md)) when there are
  several and each segment of a lane is 64 blocks or more: the caller computes the first lane and waits for the others
  at each of the four synchronization points of a pass. The result is the same on any number of workers. A call blocks
  its thread, or its worker in a task, for as long as it computes, tens of milliseconds at the defaults.
- **The memory is plain memory** from the system, never the collector's: it holds what the password makes, and may be
  gigabytes. It is zeroed before it is freed.
- **A stored string cannot ask for anything**: [verify](verify.md) refuses costs past 4 GiB of memory, 2^16 passes or
  255 lanes as `errc::unsupported`, so that a forged string makes the program spend no more than that.
- **Side channels**: Argon2i, and the first half of Argon2id's first pass, choose the blocks they read from a counter
  alone; Argon2d, and the rest of Argon2id, from the data, as RFC 9106 designs them, to resist trade-offs of time
  against memory. Every compression is additions, multiplications of 32-bit halves, XORs and rotations, and the hash
  of a PHC string is compared in constant time.
- **Errors**: options outside RFC 9106's ranges are a broken contract, `std::invalid_argument`; a string that is not
  Argon2's, or a password that does not match, is an `expected` from [verify](verify.md).

## Member types

| Type | Definition |
|---|---|
| [variant](../argon2-variant.md) | Argon2d, Argon2i, Argon2id |
| [options](../argon2-options.md) | the variant and the costs: memory, passes, lanes; a pepper, associated data |

## Member functions

| Function | Description |
|---|---|
| [derive](derive.md) | a key of `n` bytes from a password and a salt (static) |
| [derive_to](derive_to.md) | the same into the program's buffer (static) |
| [generate](generate.md) | a password hash for storage, as a PHC string with a random salt (static) |
| [verify](verify.md) | whether a password is the one of a PHC string (static) |

## Complexity

Linear in the memory times the passes: `memory` blocks of 1 KiB, each one compression G of two blocks, `iterations`
times over. On arm64 G runs on NEON with the SHA-3 extension's XAR, two of its sixteen permutations side by side, about
9 % faster than plain C++ where the memory fits the cache and 2 % at 64 MiB, where the memory's latency is most of the
cost; on x86-64 it runs on SSE2, the multiplication of the low halves one PMULUDQ. Elsewhere, and with
`SGCL_CRYPTO_PORTABLE` defined, it is plain C++; the tests run every vector on both.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string stored = crypto::argon2::generate("correct horse battery staple");
    println("{}", stored);
    println("{}", bool(crypto::argon2::verify("correct horse battery staple", stored)));
    println("{}", bool(crypto::argon2::verify("Tr0ub4dor&3", stored)));
}
```

Sample output:

```text
$argon2id$v=19$m=65536,t=3,p=4$MM+yHPs0CjbN6GkhnJFuRw$V1kQqd/JxSLX2v4q1u1sKws2/GnxqJSFH3//a3XRHpE
true
false
```

## See also

- [options](../argon2-options.md): the variant and the costs
- [pbkdf2](../pbkdf2/README.md): a key from a password where a format names PBKDF2
- [The module](../README.md)
