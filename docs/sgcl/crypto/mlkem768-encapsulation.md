[sgcl](../README.md) › [crypto](README.md) › [mlkem768](mlkem.md)

# sgcl::crypto::mlkem768::encapsulation

```cpp
#include "sgcl/crypto/mlkem.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mlkem768 {
    using encapsulation = /* unspecified */;
}

namespace sgcl::crypto::mlkem512 {
    using encapsulation = /* unspecified */;
}

namespace sgcl::crypto::mlkem1024 {
    using encapsulation = /* unspecified */;
}
```

`sgcl::crypto::mlkem768::encapsulation` is what an
[encapsulation](mlkem768-encapsulation_key/encapsulate.md) gives: the shared key, which the sender keeps, and the
ciphertext, which it sends to the owner of the key. It names a struct of these two fields and nothing else, an
aggregate that a structured binding takes apart into the two.

Each parameter set has a type of its own: `mlkem512::encapsulation` and `mlkem1024::encapsulation` are other
types, with ciphertexts of 768 and 1568 bytes, so that one set's result is never taken where another's is wanted.

## Rules

- **Move-only**, since its shared key is a secret: there is no copy, and the shared key is zeroed when the
  encapsulation goes.
- **A tracked pointer inside**: the ciphertext is a managed `vector<byte>`, so an encapsulation lives where a
  `tracked_ptr` may ([The rules of core](../core/README.md#the-rules)), on the stack above all.
- An encapsulation is made only by `encapsulate`: a `secret<32>` cannot be made by the program.

## Member objects

| Member | Description |
|---|---|
| `shared_key` | `secret<32>`: the shared key, 32 uniformly random bytes, the same the owner's `decapsulate` gives |
| `ciphertext` | `vector<byte>`: the ciphertext to send to the owner of the key, `ciphertext_size` bytes (1088); public |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    auto owner = crypto::mlkem768::decapsulation_key::generate();

    auto [shared_key, ciphertext] = owner.encapsulation_key().encapsulate();
    println("{} {}", shared_key.size, ciphertext.size());
    println("{}", owner.decapsulate(ciphertext) == shared_key);

    println("{}", std::is_copy_constructible_v<crypto::mlkem768::encapsulation>);
    println("{}", std::is_same_v<crypto::mlkem512::encapsulation,
                                 crypto::mlkem768::encapsulation>);
}
```

Output:

```text
32 1088
true
false
false
```

## See also

- [mlkem768::encapsulation_key::encapsulate](mlkem768-encapsulation_key/encapsulate.md): what makes it
- [mlkem768::decapsulation_key::decapsulate](mlkem768-decapsulation_key/decapsulate.md): the shared key of the
  ciphertext
- [secret](secret.md): the form of the shared key
- [ML-KEM](mlkem.md)
