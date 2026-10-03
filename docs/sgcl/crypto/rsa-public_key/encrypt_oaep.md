[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::encrypt_oaep

```cpp
vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message) const;           // (1)
vector<byte> encrypt_oaep(hash_id id, const slice<const byte>& message,                  // (2)
                        const slice<const byte>& label) const;
vector<byte> encrypt_oaep(hash_id id, hash_id mgf1, const slice<const byte>& message,    // (3)
                        const slice<const byte>& label) const;
```

Encrypts `message` with RSAES-OAEP (RFC 8017 §7.1) for the holder of the private key, with a fresh random seed: a
new ciphertext every time. The message and the label are bytes or text, which a slice of bytes takes both. The
message is at most [max_oaep_message_size](max_oaep_message_size.md)`(id)` bytes, the modulus's bytes less twice the
digest's less 2: 190 for a 2048-bit key and SHA-256. RSA carries a key, not a document: a longer message is
encrypted with an AEAD under a key that RSA carries.

1. The label's hash and MGF1 both by `id` (Go's `EncryptOAEP`), an empty label.
2. The same with a label, which the decryption must be given.
3. The same with MGF1 over another hash than the label's: Java's `OAEPWithSHA-256AndMGF1Padding` is SHA-256 with
   MGF1 over SHA-1.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the label, and of MGF1 in (1–2) |
| `mgf1` | the hash of MGF1 |
| `message` | the bytes to encrypt |
| `label` | bytes bound to the ciphertext, not encrypted |

## Return value

The ciphertext, [size](size.md) bytes.

## Complexity

Linear in the bits of the modulus times the bits of e, and in the length of the label.

## Exceptions

- `invalid_argument` when the message is longer than [max_oaep_message_size](max_oaep_message_size.md)`(id)`, when
  the key is too small for the hash at all (fewer bytes than twice the digest and 2: a 1024-bit key with SHA-512,
  where even an empty message is refused), or when `id` or `mgf1` is not a [hash_id](../hash_id.md) the module has.
- `logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    crypto::rsa::public_key pub = key.public_key();
    auto sealed = pub.encrypt_oaep(crypto::hash_id::sha256, "the session key", "v1");
    println("{} bytes", sealed.size());
    auto opened = key.decrypt_oaep(crypto::hash_id::sha256, sealed, "v1");
    println("{}", string(opened.value()));

    // Java's OAEPWithSHA-256AndMGF1Padding: MGF1 over SHA-1
    auto id = crypto::hash_id::sha256;
    auto java = pub.encrypt_oaep(id, crypto::hash_id::sha1, "the session key", "");
    println("{}", key.decrypt_oaep(id, crypto::hash_id::sha1, java, "").has_value());
    try {
        pub.encrypt_oaep(crypto::hash_id::sha256, vector<byte>(191));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
256 bytes
the session key
true
sgcl::crypto::rsa: a message too long for OAEP under this key and hash
```

## See also

- [decrypt_oaep](../rsa-private_key/decrypt_oaep.md): the decryption
- [max_oaep_message_size](max_oaep_message_size.md): the longest message
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
