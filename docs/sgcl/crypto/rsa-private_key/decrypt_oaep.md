[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::decrypt_oaep

```cpp
[[nodiscard]] expected<vector<byte>, error>
decrypt_oaep(hash_id id, const slice<const byte>& ciphertext) const;           // (1)
[[nodiscard]] expected<vector<byte>, error>
decrypt_oaep(hash_id id, const slice<const byte>& ciphertext,                  // (2)
             const slice<const byte>& label) const;
[[nodiscard]] expected<vector<byte>, error>
decrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& ciphertext,    // (3)
             const slice<const byte>& label) const;
```

Returns the message of an RSAES-OAEP ciphertext (RFC 8017 §7.1.2) that
[public_key::encrypt_oaep](../rsa-public_key/encrypt_oaep.md) made for this key, with the hashes and the label it was
made with.

1. The label's hash and MGF1 both by `id`, an empty label.
2. The same with the label (bytes or text) the encryption was given.
3. The same with MGF1 over another hash than the label's.

Every failure — a ciphertext of another length than [size](size.md) (exactly the modulus's bytes, as RFC 8017 §7.1.2
has it, where Go and OpenSSL take a shorter one), one not below n, an encoding that is not OAEP's, another label,
another hash — is the same [crypto::error](../error.md): `errc::authentication`, "sgcl::crypto::rsa: decryption
error", offset 0, in the same time: every check is done on every byte and folded into one mask before the one branch.
An attacker who can tell a bad leading byte from a bad label hash decrypts any message (Manger, CRYPTO 2001). A fault
the check with the public exponent finds is that error too.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the label, and of MGF1 in (1–2) |
| `mgf1` | the hash of MGF1 |
| `ciphertext` | the ciphertext, [size](size.md) bytes |
| `label` | the label the encryption was given |

## Return value

The message, a `vector<byte>` in managed memory: it is the user's data and nobody zeroes it. Or the one decryption
error.

## Complexity

Cubic in the bits of the modulus, as [sign_digest](sign_digest.md).

## Exceptions

- `invalid_argument` when `id` or `mgf1` is not a [hash_id](../hash_id.md) the module has.
- `logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto sealed = key.public_key().encrypt_oaep(crypto::hash_id::sha256, "meet at noon", "chat");
    auto opened = key.decrypt_oaep(crypto::hash_id::sha256, sealed, "chat");
    println("{}", string(opened.value()));

    auto unlabelled = key.decrypt_oaep(crypto::hash_id::sha256, sealed);
    println("{}", unlabelled.error().message());
    sealed[100] ^= byte(1);
    println("{}", key.decrypt_oaep(crypto::hash_id::sha256, sealed, "chat").error().message());
}
```

Output:

```text
meet at noon
sgcl::crypto::rsa: decryption error
sgcl::crypto::rsa: decryption error
```

## See also

- [decrypt_oaep_to](decrypt_oaep_to.md): the message into the program's buffer
- [public_key::encrypt_oaep](../rsa-public_key/encrypt_oaep.md): the encryption
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
