[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [encapsulation_key](../mlkem768-encapsulation_key.md)

# sgcl::crypto::mlkem768::encapsulation_key::encapsulate

```cpp
encapsulation encapsulate() const noexcept;
```

Makes a fresh shared key and the ciphertext that carries it to the owner of the key (FIPS 203's ML-KEM.Encaps), Go's
`Encapsulate`: 32 bytes m from [crypto::random](../random.md), and from m and the key the shared key K and the
ciphertext c. The owner's [decapsulate](../mlkem768-decapsulation_key/decapsulate.md) of the ciphertext gives the
same K. Every call draws a new m, so two encapsulations to one key give two different keys and ciphertexts.

`mlkem512::encapsulation_key::encapsulate` gives a `mlkem512::encapsulation` with a ciphertext of 768 bytes,
`mlkem1024`'s a `mlkem1024::encapsulation` with one of 1568 bytes; the shared key is 32 bytes in every set.

## Parameters

None.

## Return value

An [encapsulation](../mlkem768-encapsulation.md): `shared_key`, a `secret<32>`, and `ciphertext`, `ciphertext_size`
bytes (1088) to send to the owner of the key.

## Complexity

Constant: the encryption of FIPS 203's K-PKE with the key's matrix, made when the key was.

## Exceptions

None.

## Notes

m and everything made from it are secret: the arithmetic runs in constant time, and m is zeroed before the function
returns. The shared key is zeroed when the `secret<32>` goes; the ciphertext is public and lies in managed memory.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto owner = crypto::mlkem768::decapsulation_key::generate();
    auto key = owner.encapsulation_key();

    auto first = key.encapsulate();
    auto second = key.encapsulate();
    println("{} {}", first.shared_key.size, first.ciphertext.size());
    println("{}", first.ciphertext == second.ciphertext);
    println("{}", owner.decapsulate(first.ciphertext) == first.shared_key);
    println("{}", owner.decapsulate(second.ciphertext) == second.shared_key);
}
```

Output:

```text
32 1088
false
true
true
```

## See also

- [mlkem768::decapsulation_key::decapsulate](../mlkem768-decapsulation_key/decapsulate.md): the shared key of a
  ciphertext
- [mlkem768::encapsulation](../mlkem768-encapsulation.md): what this gives
- [sgcl::crypto::mlkem768::encapsulation_key](../mlkem768-encapsulation_key.md)
