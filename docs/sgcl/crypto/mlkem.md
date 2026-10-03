[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::mlkem512, mlkem768, mlkem1024

```cpp
#include "sgcl/crypto/mlkem.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mlkem768 {
    class encapsulation_key;
    class decapsulation_key;
    using encapsulation = /* unspecified */;

    inline constexpr size_t encapsulation_key_size = 1184;
    inline constexpr size_t ciphertext_size = 1088;
    inline constexpr size_t seed_size = 64;
    inline constexpr size_t shared_key_size = 32;
}

namespace sgcl::crypto::mlkem512 {
    class encapsulation_key;
    class decapsulation_key;
    using encapsulation = /* unspecified */;

    inline constexpr size_t encapsulation_key_size = 800;
    inline constexpr size_t ciphertext_size = 768;
    inline constexpr size_t seed_size = 64;
    inline constexpr size_t shared_key_size = 32;
}

namespace sgcl::crypto::mlkem1024 {
    class encapsulation_key;
    class decapsulation_key;
    using encapsulation = /* unspecified */;

    inline constexpr size_t encapsulation_key_size = 1568;
    inline constexpr size_t ciphertext_size = 1568;
    inline constexpr size_t seed_size = 64;
    inline constexpr size_t shared_key_size = 32;
}
```

ML-KEM (FIPS 203), Go's `crypto/mlkem`: a key encapsulation mechanism on module lattices, believed secure against a
quantum computer as well as a classical one. The owner of a
[decapsulation_key](mlkem768-decapsulation_key/README.md) publishes its
[encapsulation_key](mlkem768-encapsulation_key/README.md); anyone encapsulates to it, which gives a 32-byte shared key and
a ciphertext (an [encapsulation](mlkem768-encapsulation.md)); the ciphertext sent to the owner decapsulates to the
same shared key. TLS 1.3 pairs ML-KEM-768 with [X25519](x25519.md) (X25519MLKEM768, in
[net::tls](../net/tls/README.md)), so that the exchange holds while either of the two does.

There are three parameter sets, each a namespace of the same three types and four constants: `mlkem512` (security
category 1), `mlkem768` (category 3, the one to use) and `mlkem1024` (category 5). The types are documented once,
under `mlkem768`; the sets differ in their sizes alone. A type of one set is never taken for another's:
`mlkem512::encapsulation` and `mlkem768::encapsulation` are different types, and so are the keys.

Written from FIPS 203, tested against Wycheproof's vectors of the three sets (key generation from a seed,
encapsulation with a given message, decapsulation with the implicit rejection, expanded keys), against OpenSSL on
100 000 random cases of each set and against Go's `crypto/mlkem` on 100 000 of ML-KEM-768 and of ML-KEM-1024 (Go
has no 512), byte for byte, the rejected ciphertexts included. **The implementation has not been through an
independent cryptographic audit.**

## Rules

- **The sizes** of the sets are the constants of their namespaces, in bytes:

  | Constant | mlkem512 | mlkem768 | mlkem1024 | Description |
  |---|---|---|---|---|
  | `encapsulation_key_size` | 800 | 1184 | 1568 | the encapsulation key, as `bytes()` gives it and `from_bytes` takes it |
  | `ciphertext_size` | 768 | 1088 | 1568 | the ciphertext of an encapsulation, the only length `decapsulate` takes |
  | `seed_size` | 64 | 64 | 64 | the seed d‖z a decapsulation key is kept as |
  | `shared_key_size` | 32 | 32 | 32 | the shared key, a `secret<32>` |

- **The shared key is a key**, 32 uniformly random bytes, as FIPS 203 promises: it may key a cipher as it stands, or
  go through [hkdf](hkdf/README.md) with the rest of a protocol's transcript, as TLS does.
- **A wrong ciphertext is no error.** A ciphertext of the right length that is not a genuine one decapsulates to a
  pseudorandom key (the implicit rejection of FIPS 203 §6.3), in the same time as a genuine one: the protocol finds
  out when the keys do not match, and an attacker learns nothing from the timing. Only a ciphertext of another
  length is `errc::malformed`.
- **An encapsulation key is a public value, checked when it is read.** `from_bytes` refuses another length and a
  key with a coefficient not below q = 3329 (FIPS 203 §7.2) with `errc::invalid_key`. The key keeps the matrix Â
  its encapsulations need, made once when it is read, as Go and OpenSSL keep it: 2, 4.5 or 8 KB for the three sets,
  so a key is passed by `const&` or kept once, since a copy copies the matrix.
- **A decapsulation key is a secret, kept as its seed.** The seed d‖z of 64 bytes (FIPS 203 §7.1, the form Go keeps)
  and the expanded key made from it once live in the object's own memory, never in managed memory, which the
  collector frees without zeroing. The key is move-only, `clone()` makes a second one, a move leaves the object moved
  from zeroed, the destructor zeroes the whole object with stores the compiler cannot drop
  ([secure_zero](secure_zero.md)), and an object moved from refuses every operation with `std::logic_error`. Keep a
  key on the stack or in a `unique_ptr`. The shared key and `seed()` are [secret\<N\>](secret/README.md), which zero
  themselves in turn. There is no form of the expanded key and no DER: those come with TLS and X.509's certificates
  of ML-KEM.
- **What is secret**: the seed, the vector s and the errors made from it, the message m, everything derived from
  them, the shared key. **Constant time** wherever they are: the arithmetic of the ring neither branches nor divides
  on a value (Compress, whose definition divides by q, is a multiplication and a shift); decapsulation computes the
  genuine key and the rejection's and chooses between them with a mask on a comparison of every byte; what held a
  secret on the way is zeroed before a function returns. What is public — the encapsulation key, the matrix it makes
  (its rejection sampling takes the time the key's bytes ask for), a ciphertext's length — may take its own time.
  dudect finds no dependence on the secret (|t| below 2 on the machine of the tests); the machine code has no
  division in any function the secrets go through.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the owner's key, from a seed of 64 bytes (a program makes its own with generate())
    auto owner = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));

    // the owner publishes its encapsulation key, 1184 bytes
    auto published = owner->encapsulation_key().bytes();
    println("encapsulation key: {} bytes, {}...", published.size(),
            encoding::hex::encode(published).substr(0, 16));

    // anyone reads it and encapsulates: a shared key, and a ciphertext to send
    auto recipient = crypto::mlkem768::encapsulation_key::from_bytes(published);
    auto sent = recipient->encapsulate();
    println("ciphertext: {} bytes", sent.ciphertext.size());

    // the owner decapsulates the ciphertext to the same key
    auto received = owner->decapsulate(sent.ciphertext);
    println(received == sent.shared_key ? "the same key" : "different keys");

    // a ciphertext changed on the way gives another key, not an error
    auto changed = sent.ciphertext;
    changed[0] ^= byte{1};
    auto other = owner->decapsulate(changed);
    println(other == sent.shared_key ? "the same key" : "another key");

    // one of the wrong length is refused
    auto refused = owner->decapsulate(vector<byte>(100));
    if (!refused) {
        println(refused.error().message());
    }
}
```

Output:

```text
encapsulation key: 1184 bytes, 298aa10d423c8dda...
ciphertext: 1088 bytes
the same key
another key
sgcl::crypto::mlkem768: a ciphertext of the wrong length
```

## See also

- [mlkem768::decapsulation_key](mlkem768-decapsulation_key/README.md): the owner's secret key
- [mlkem768::encapsulation_key](mlkem768-encapsulation_key/README.md): the published key
- [mlkem768::encapsulation](mlkem768-encapsulation.md): the shared key and the ciphertext
- [x25519](x25519.md): the classical exchange TLS pairs it with
- [hkdf](hkdf/README.md), [random](random/README.md), [secret](secret/README.md), [error](error/README.md)
- [README: The rules](README.md#the-rules)
