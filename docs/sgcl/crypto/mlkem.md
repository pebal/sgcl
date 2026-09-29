# sgcl::crypto::mlkem512, mlkem768, mlkem1024

```cpp
#include "sgcl/crypto/mlkem.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto::mlkem768 {        // and mlkem512, mlkem1024: the same names
    class decapsulation_key;   // the secret side: a seed of 64 bytes and the key made from it; move-only
    class encapsulation_key;   // the public side, 1184 bytes; a value
    struct encapsulation;      // what encapsulation gives: shared_key (secret<32>) and ciphertext

    inline constexpr size_t encapsulation_key_size = 1184;   // 800 / 1184 / 1568
    inline constexpr size_t ciphertext_size = 1088;          // 768 / 1088 / 1568
    inline constexpr size_t seed_size = 64;
    inline constexpr size_t shared_key_size = 32;
}
```

ML-KEM (FIPS 203), Go's `crypto/mlkem`: a key encapsulation mechanism on module lattices, believed secure against a quantum computer as well as a classical one. The owner of a decapsulation key publishes its encapsulation key; anyone encapsulates to it, which gives a 32-byte shared key and a ciphertext; the ciphertext sent to the owner decapsulates to the same shared key. TLS 1.3 pairs ML-KEM-768 with X25519 (X25519MLKEM768), so that the exchange holds while either of the two does. Three parameter sets, each a namespace of the same types: `mlkem512` (security category 1), `mlkem768` (3, the one to use), `mlkem1024` (5).

Written from FIPS 203, tested against Wycheproof's vectors of the three sets (key generation from a seed, encapsulation with a given message, decapsulation with the implicit rejection, expanded keys), against OpenSSL on 100 000 random cases of each set and against Go's `crypto/mlkem` on 100 000 of ML-KEM-768 and of ML-KEM-1024 (Go has no 512), byte for byte, the rejected ciphertexts included.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shared key is a key**, 32 uniformly random bytes, as FIPS 203 promises: it may key a cipher as it stands, or go through [`hkdf`](hkdf.md) with the rest of a protocol's transcript, as TLS does.
- **A wrong ciphertext is no error.** A ciphertext of the right length that is not a genuine one decapsulates to a pseudorandom key (the implicit rejection of FIPS 203 §6.3), in the same time as a genuine one: the protocol finds out when the keys do not match, and an attacker learns nothing from the timing. Only a ciphertext of another length is `errc::malformed`.
- **An encapsulation key is checked when it is read.** `encapsulation_key::from_bytes` refuses another length and a key with a coefficient not below q = 3329 (FIPS 203 §7.2) with `errc::invalid_key`.
- **An encapsulation key is a value of 2, 4.5 or 8 KB** (ML-KEM-512, -768, -1024). It holds the key's bytes and the matrix Â its encapsulations need, which `from_bytes` makes once when it reads the key (Go and OpenSSL keep it too); an encapsulation then costs 23 to 42 per cent less, and reading the key costs what OpenSSL's and Go's reading does. Pass it by `const&` or keep it once: a copy copies the matrix.
- **The decapsulation key is kept as its seed.** `seed()` gives the 64 bytes d‖z (FIPS 203 §7.1, the form Go keeps), `from_seed` makes the key from them again; `generate()` takes them from [`random`](random.md). There is no form of the expanded key and no DER: those come with TLS and X.509's certificates of ML-KEM.
- **A secret, so no copy.** `decapsulation_key` holds the seed, the expanded key and its matrix in the object's own memory — never in managed memory; it is move-only, `clone()` makes a second one, a move leaves the object moved from zeroed, the destructor zeroes the whole object with stores the compiler cannot drop, and an object moved from refuses every operation with `std::logic_error`. Keep a key on the stack or in a `unique_ptr`. The shared key and `seed()` are [`secret<N>`](secret.md), which zero themselves in turn.
- **Constant time.** The arithmetic of the ring neither branches nor divides on a value (Compress, whose definition divides by q, is a multiplication and a shift); decapsulation computes the genuine key and the rejection's and chooses between them with a mask on a comparison of every byte; what held a secret on the way is zeroed before a function returns. What is public — the encapsulation key, the matrix it makes (its rejection sampling takes the time the key's bytes ask for), a ciphertext's length — may take its own time. dudect finds no dependence on the secret (|t| below 2 on this machine); the machine code has no division in any function the secrets go through.

## Members

```cpp
class decapsulation_key {
public:
    static decapsulation_key generate();
    static expected<decapsulation_key, error> from_seed(const slice<const byte>& seed);   // 64 bytes

    decapsulation_key(decapsulation_key&& other) noexcept;              // other zeroed
    decapsulation_key& operator=(decapsulation_key&& other) noexcept;
    decapsulation_key(const decapsulation_key&) = delete;
    ~decapsulation_key();                                               // zeroed
    decapsulation_key clone() const;

    secret<64> seed() const;
    mlkem768::encapsulation_key encapsulation_key() const;
    expected<secret<32>, error> decapsulate(const slice<const byte>& ciphertext) const;
    friend bool operator==(const decapsulation_key& a, const decapsulation_key& b) noexcept;   // constant time
};

class encapsulation_key {
public:
    static expected<encapsulation_key, error> from_bytes(const slice<const byte>& bytes);
    vector<byte> bytes() const;
    encapsulation encapsulate() const;                                  // m from crypto::random
    friend bool operator==(const encapsulation_key& a, const encapsulation_key& b) noexcept;
};

struct encapsulation {
    secret<32> shared_key;
    vector<byte> ciphertext;
};
```

`mlkem512::encapsulation` and `mlkem768::encapsulation` are different types: one set's result is not taken where another's is wanted.

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

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

[`x25519`](x25519.md), the classical exchange TLS pairs it with; [`hkdf`](hkdf.md); [`random`](random.md); [`secret`](secret.md); [`secure_zero`](secure_zero.md); [`error`](error.md); the [benchmarks](benchmarks.md).
