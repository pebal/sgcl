[sgcl](../../../README.md) › [crypto](../../README.md) › [aead](README.md)

# sgcl::crypto::mixin::aead\<Derived\>::seal

```cpp
vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext) const;    // (1)
vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext,           // (2)
                  const slice<const byte>& aad) const;
```

Encrypts `plaintext` under the key of the object and `nonce`, and authenticates the ciphertext together with the
additional data: the result is the ciphertext followed by the tag, what Go's `aead.Seal(nil, nonce, plaintext,
aad)` gives.

1. Without additional data.
2. With `aad`, which is authenticated but neither encrypted nor part of the result: the receiver gives the same
   bytes to [open](open.md), and a message opened with other additional data is a forgery.

## Parameters

| Parameter | Description |
|---|---|
| `nonce` | exactly `nonce_size` bytes of the class, never used twice under one key ([nonce_counter](../../nonce_counter/README.md)) |
| `plaintext` | the bytes to encrypt, at most `max_plaintext_size` of the class |
| `aad` | the additional data: a header sent in the clear, a record's number, a file's name |

## Return value

A new `vector<byte>` of `plaintext.size() + tag_size` bytes: the ciphertext, as long as the plaintext, then the
tag. The nonce is not in it.

## Complexity

Linear in the sizes of `plaintext` and `aad`.

## Exceptions

- `invalid_argument` when `nonce` is not `nonce_size` bytes.
- `length_error` when `plaintext` is longer than `max_plaintext_size`.
- `logic_error` when the object was moved from and holds no key.

Nothing is sealed then.

## Notes

The vector is managed memory, as any of the program's data: a ciphertext is not a secret. [seal_to](seal_to.md)
writes into a buffer the program gives, with nothing allocated.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The test cases 2 and 4 of the GCM specification (McGrew and Viega)
    crypto::aes_gcm zero_key(vector<byte>(16));
    auto sealed = zero_key.seal(vector<byte>(12), vector<byte>(16));
    println("{}", encoding::hex::encode(sealed));

    vector<byte> key = encoding::hex::decode("feffe9928665731c6d6a8f9467308308");
    vector<byte> nonce = encoding::hex::decode("cafebabefacedbaddecaf888");
    vector<byte> text = encoding::hex::decode(
        "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
        "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39");
    vector<byte> aad = encoding::hex::decode("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    crypto::aes_gcm gcm(key);
    auto with_aad = gcm.seal(nonce, text, aad);
    println("{} bytes, tag {}", with_aad.size(), encoding::hex::encode(with_aad.as_slice(60, 16)));

    try {
        gcm.seal(nonce.as_slice(0, 8), text);
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf
76 bytes, tag 5bc94fbc3221a5db94fae95ae7121a47
sgcl::crypto::aes_gcm: a nonce of 8 bytes, not 12
```

## See also

- [open](open.md): checks the tag and decrypts
- [seal_to](seal_to.md): into the caller's buffer, in place too
- [xchacha20_poly1305::seal_random](../../xchacha20_poly1305/seal_random.md): a random nonce, written in front
- [sgcl::crypto::mixin::aead\<Derived\>](README.md)
