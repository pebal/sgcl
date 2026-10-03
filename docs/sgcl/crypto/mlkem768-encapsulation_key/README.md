[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md)

# sgcl::crypto::mlkem768::encapsulation_key

```cpp
#include "sgcl/crypto/mlkem.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mlkem768 {
    class encapsulation_key;
}

namespace sgcl::crypto::mlkem512 {
    class encapsulation_key;
}

namespace sgcl::crypto::mlkem1024 {
    class encapsulation_key;
}
```

`sgcl::crypto::mlkem768::encapsulation_key` is the public half of an [ML-KEM](../mlkem.md) key pair, Go's
`mlkem.EncapsulationKey768`: the key the owner of a [decapsulation_key](../mlkem768-decapsulation_key/README.md) publishes, and
anyone [encapsulates](encapsulate.md) to, which gives a shared key and the ciphertext to
send to the owner. It is read from its bytes by [from_bytes](from_bytes.md) and taken
from the owner's key by [encapsulation_key()](../mlkem768-decapsulation_key/encapsulation_key.md); there is no other
way to make one.

The key is a plain value: copied, compared, kept anywhere. Besides its bytes it keeps the matrix Â that every
encapsulation needs, made once when the key is made, as Go and OpenSSL keep it. `mlkem512::encapsulation_key` and
`mlkem1024::encapsulation_key` are the same class over the other parameter sets: their keys are 800 and 1568 bytes
(`encapsulation_key_size`), their ciphertexts 768 and 1568 bytes, and their matrices 2 and 8 KB where ML-KEM-768's is
4.5 KB.

## Rules

- **Public**: nothing in the key is a secret; its comparison and the reading of its bytes take the time the bytes
  ask for.
- **Checked when it is read**: a key of another length, or with a coefficient not below q = 3329 (FIPS 203 §7.2), is
  refused with `errc::invalid_key`, so a key that exists is one an encapsulation may use.
- **Passed by `const&` or kept once**: the object holds the matrix, 5.7 KB in all for ML-KEM-768 (2.8 KB for 512,
  9.6 KB for 1024), and a copy copies it. It holds no tracked pointer: it lives anywhere, a global and a `std`
  container included.
- **Thread safety**: the methods are `const` and an encapsulation draws its own random bytes, so any number of threads
  may encapsulate to one key at once.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the copy and the move; a key is made by `from_bytes` or by a decapsulation key's `encapsulation_key()` |

#### Creation

| Function | Description |
|---|---|
| [from_bytes](from_bytes.md) | reads a key from its bytes, checked (static) |

#### Encapsulation

| Function | Description |
|---|---|
| [encapsulate](encapsulate.md) | a fresh shared key and the ciphertext that carries it |

#### Encodings

| Function | Description |
|---|---|
| [bytes](bytes.md) | the key's bytes, the form it is published in |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares the bytes of two keys |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the owner's key, from a seed of the tests; a program makes its own with generate()
    auto owner = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));
    vector<byte> published = owner->encapsulation_key().bytes();

    // the other side reads the key once and encapsulates to it
    auto peer = crypto::mlkem768::encapsulation_key::from_bytes(published);
    if (!peer) {
        eprintln(peer.error().message());
        return 1;
    }
    auto [shared_key, ciphertext] = peer->encapsulate();
    println("{} bytes to send", ciphertext.size());

    // the owner gets the same shared key from the ciphertext
    println("{}", owner->decapsulate(ciphertext) == shared_key);
}
```

Output:

```text
1088 bytes to send
true
```

## See also

- [mlkem768::decapsulation_key](../mlkem768-decapsulation_key/README.md): the owner's secret key
- [mlkem768::encapsulation](../mlkem768-encapsulation.md): what an encapsulation gives
- [ML-KEM](../mlkem.md): the parameter sets, the sizes, the rules
- [x25519::public_key](../x25519-public_key/README.md): the classical counterpart
