[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::decrypt_oaep_to

```cpp
[[nodiscard]] expected<size_t, error>
decrypt_oaep_to(const slice<byte>& out, hash_id id,                  // (1)
                const slice<const byte>& ciphertext) const;
[[nodiscard]] expected<size_t, error>
decrypt_oaep_to(const slice<byte>& out, hash_id id,                  // (2)
                const slice<const byte>& ciphertext,
                const slice<const byte>& label) const;
[[nodiscard]] expected<size_t, error>
decrypt_oaep_to(const slice<byte>& out, hash_id id, hash_id mgf1,    // (3)
                const slice<const byte>& ciphertext,
                const slice<const byte>& label) const;
```

Writes the message of an OAEP ciphertext into `out`, the program's own buffer, which it clears with
[secure_zero](../secure_zero.md) when done: for a key unwrapped or a password, data that must not stay in managed
memory, as an AEAD's `open_to`. `out` holds at least
[max_oaep_message_size](../rsa-public_key/max_oaep_message_size.md)`(id)` bytes, decided before anything is
decrypted, from the key and the hash alone; on a failure `out` is not touched. The overloads take the hashes and
the label as those of [decrypt_oaep](decrypt_oaep.md) do.

Every failure — a ciphertext of another length than [size](size.md) (exactly the modulus's bytes, as RFC 8017 §7.1.2
has it, where Go and OpenSSL take a shorter one), one not below n, an encoding that is not OAEP's, another label,
another hash — is the same [crypto::error](../error/README.md): `errc::authentication`, "sgcl::crypto::rsa: decryption
error", offset 0, in the same time: every check is done on every byte and folded into one mask before the one branch.
An attacker who can tell a bad leading byte from a bad label hash decrypts any message (Manger, CRYPTO 2001). A fault
the check with the public exponent finds is that error too.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the message is written |
| `id` | the hash of the label, and of MGF1 in (1–2) |
| `mgf1` | the hash of MGF1 |
| `ciphertext` | the ciphertext, [size](size.md) bytes |
| `label` | the label the encryption was given |

## Return value

The number of bytes written at the start of `out`, or the one decryption error.

## Complexity

Cubic in the bits of the modulus, as [sign_digest](sign_digest.md).

## Exceptions

- `length_error` when `out` holds fewer bytes than
  [max_oaep_message_size](../rsa-public_key/max_oaep_message_size.md)`(id)`.
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

    auto session_key = crypto::random::secret(32);
    auto wrapped = key.public_key().encrypt_oaep(crypto::hash_id::sha256, session_key);
    crypto::secret_bytes unwrapped(key.public_key().max_oaep_message_size(crypto::hash_id::sha256));
    auto n = key.decrypt_oaep_to(unwrapped, crypto::hash_id::sha256, wrapped);
    println("{} of {} bytes", n.value(), unwrapped.size());
}
```

Output:

```text
32 of 190 bytes
```

## See also

- [decrypt_oaep](decrypt_oaep.md): the message as a `vector<byte>`
- [secret_bytes](../secret_bytes/README.md): a buffer of the program's that is zeroed when it goes
- [sgcl::crypto::rsa::private_key](README.md)
