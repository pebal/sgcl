[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::bcrypt

```cpp
#include "sgcl/crypto/bcrypt.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class bcrypt;
}
```

`sgcl::crypto::bcrypt` is bcrypt of Provos and Mazières (USENIX 1999), Go's `golang.org/x/crypto/bcrypt`: the password
hash of OpenBSD and of most web frameworks of the last twenty years. It keys Blowfish 2^cost times by the password and
a salt and then encrypts a constant with it; a hash is OpenBSD's string of 60 characters, `$2b$10$` and 22 characters
of salt and 31 of hash in bcrypt's own base64. [generate](generate.md) makes one with a random salt and
[verify](verify.md) checks a password against one; [cost](cost.md) reads its cost, to make a new hash at a login when
the program has raised it.

bcrypt costs time but almost no memory (4 KiB), so a graphics card guesses it far more cheaply than
[argon2](../argon2/README.md), which is the better choice for a new system; bcrypt is here for the hashes that exist
and the systems that ask for it.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **72 bytes of a password count.** bcrypt reads at most 72 bytes of a password; [generate](generate.md) refuses a
  longer one with `errc::invalid_key`, as Go does, rather than make two passwords one, and [verify](verify.md) reads
  its first 72, as every implementation does, so that the hashes other programs made of long passwords verify. A
  program that takes passphrases longer than that hashes them first (with [sha256](../sha256/README.md), in base64)
  or uses [argon2](../argon2/README.md).
- **Prefixes**: `$2a$`, `$2b$` and `$2y$` are read as one function — the 72 bytes are cut before their length is
  taken, so `$2a$`'s old wrap-around past 255 bytes cannot occur; `$2b$` is written. `$2x$` (crypt_blowfish's
  sign-extension bug) and `$2$` are refused as `errc::unsupported`.
- **The cost** is 4 to 31, 10 by default (Go's); each step doubles the time. A cost outside the range is a broken
  contract, `std::invalid_argument`.
- **Side channels**: Blowfish reads its S-boxes at places made of the key, as every bcrypt does; the hashes are
  compared in constant time.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `min_cost` | `4` | the smallest cost, `static constexpr int` |
| `max_cost` | `31` | the largest cost, `static constexpr int` |
| `default_cost` | `10` | the cost of [generate](generate.md) when none is given, Go's `DefaultCost`, `static constexpr int` |
| `max_password_size` | `72` | the bytes of a password bcrypt reads, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [cost](cost.md) | the cost a hash was made with (static) |
| [generate](generate.md) | a hash of a password for storage, with a random salt (static) |
| [verify](verify.md) | whether a password is the one of a hash (static) |

## Complexity

Linear in 2^cost: 1042 Blowfish encryptions a round, two rounds of key setup for each of the 2^cost. One path, plain
C++: a cipher of table lookups in a chain has nothing for vector units.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string stored = crypto::bcrypt::generate("correct horse battery staple");
    println("{}", stored);
    println("{}", bool(crypto::bcrypt::verify("correct horse battery staple", stored)));
    println("{}", bool(crypto::bcrypt::verify("Tr0ub4dor&3", stored)));
}
```

Sample output:

```text
$2b$10$9P0OVVc34zlwkPkKDbv5j.9iTqCfJyOAo9CVBIrqZXSY4oiHIv/Im
true
false
```

## See also

- [argon2](../argon2/README.md): the memory-hard password hash
- [The module](../README.md)
