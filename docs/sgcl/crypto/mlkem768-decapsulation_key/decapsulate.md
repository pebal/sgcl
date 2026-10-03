[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](../mlkem768-decapsulation_key.md)

# sgcl::crypto::mlkem768::decapsulation_key::decapsulate

```cpp
expected<secret<32>, error> decapsulate(const slice<const byte>& ciphertext) const;
```

The shared key of a ciphertext sent to this key (FIPS 203's ML-KEM.Decaps), Go's `Decapsulate`: the key the
sender's [encapsulate](../mlkem768-encapsulation_key/encapsulate.md) made. A ciphertext of the right length that is
not a genuine one, made for another key or changed on the way, is no error: it decapsulates to a pseudorandom key
derived from z and the ciphertext (the implicit rejection of FIPS 203 §6.3), in the same time as a genuine one. The
protocol finds out when the two sides' keys do not match, and an attacker learns nothing from the answer or its
timing.

`mlkem512::decapsulation_key::decapsulate` takes a ciphertext of 768 bytes, `mlkem1024`'s one of 1568; the shared
key is 32 bytes in every set.

## Parameters

| Parameter | Description |
|---|---|
| `ciphertext` | the ciphertext, `ciphertext_size` bytes (1088) |

## Return value

The shared key, a [secret\<32\>](../secret.md), or a [crypto::error](../error.md) with `errc::malformed` for a
ciphertext of another length.

## Complexity

Constant: the decryption of K-PKE, its encryption again with the key's matrix, and a comparison of every byte of the
two ciphertexts.

## Exceptions

`std::logic_error` when the key was moved from.

## Notes

Both keys, the genuine one and the rejection's, are computed every time, and one of them is chosen by a mask on the
comparison: no branch and no address depends on whether the ciphertext was genuine. What held a secret on the way is
zeroed before the function returns.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));

    auto sent = key->encapsulation_key().encapsulate();
    println("{}", key->decapsulate(sent.ciphertext) == sent.shared_key);

    // a ciphertext of zeros is no genuine one: the key of the implicit rejection,
    // the same in every implementation of FIPS 203
    auto rejected = key->decapsulate(vector<byte>(crypto::mlkem768::ciphertext_size));
    println("{}...", encoding::hex::encode(rejected->bytes()).substr(0, 16));

    auto refused = key->decapsulate(vector<byte>(100));
    println("{}", refused.error().message());
}
```

Output:

```text
true
c8fbeddafdacef2f...
sgcl::crypto::mlkem768: a ciphertext of the wrong length
```

## See also

- [mlkem768::encapsulation_key::encapsulate](../mlkem768-encapsulation_key/encapsulate.md): the shared key and its
  ciphertext
- [ML-KEM](../mlkem.md): the implicit rejection and what is secret
- [sgcl::crypto::mlkem768::decapsulation_key](../mlkem768-decapsulation_key.md)
