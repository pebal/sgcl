[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::decrypt

```cpp
[[nodiscard]] expected<vector<byte>, error> decrypt(const slice<const byte>& ciphertext);
```

Decrypts `ciphertext`, after what the object decrypted before, and takes its PKCS #7 padding off. The padding is
checked over the whole last block in a time that does not depend on which byte is wrong, and every wrong padding is
one error with one message; what was decrypted is zeroed before the error is returned. A wrong padding means another
key or IV, or a changed ciphertext — and an answer that reveals it to the sender is a padding oracle: check a MAC
over the ciphertext first.

## Parameters

| Parameter | Description |
|---|---|
| `ciphertext` | whole blocks, one or more |

## Return value

The message, or an [error](../error/README.md): `errc::malformed` when `ciphertext` is empty or not whole blocks,
`errc::authentication` when its padding is not PKCS #7's. `[[nodiscard]]`: a decryption whose result is dropped hides
the error.

## Complexity

Linear in `ciphertext.size()`; eight blocks at a time on the AES instructions.

## Exceptions

`logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(16), iv(16);
    crypto::aes_cbc encryptor(key, iv);
    vector<byte> sealed = encryptor.encrypt("attack at dawn");

    crypto::aes_cbc decryptor(key, iv);
    println("{}", string(decryptor.decrypt(sealed).value()));

    sealed.back() ^= byte(1);  // the padding no longer checks
    decryptor.reset(iv);
    println("{}", decryptor.decrypt(sealed).error().message());
}
```

Output:

```text
attack at dawn
sgcl::crypto::aes_cbc: the padding is not PKCS #7's
```

## See also

- [encrypt](encrypt.md): the ciphertext
- [decrypt_blocks](decrypt_blocks.md): whole blocks, no padding
- [sgcl::crypto::aes_cbc](README.md)
