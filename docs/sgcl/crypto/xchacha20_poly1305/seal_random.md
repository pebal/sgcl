[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](../xchacha20_poly1305.md)

# sgcl::crypto::xchacha20_poly1305::seal_random

```cpp
vector<byte> seal_random(const slice<const byte>& plaintext) const;    // (1)
vector<byte> seal_random(const slice<const byte>& plaintext,           // (2)
                         const slice<const byte>& aad) const;
```

Draws a nonce of 24 bytes from the system's generator and seals `plaintext` under it, the nonce written before
the ciphertext: what a Go program writes as a random nonce and `append(nonce, aead.Seal(nil, nonce, plaintext,
aad)...)`. The one AEAD of the module where a random nonce is safe for any number of messages: 192 random bits do
not collide.

1. Without additional data.
2. With `aad`, authenticated but neither encrypted nor part of the result.

## Parameters

| Parameter | Description |
|---|---|
| `plaintext` | the bytes to encrypt, at most `max_plaintext_size` |
| `aad` | the additional data: a header sent in the clear, a record's number, a file's name |

## Return value

A new `vector<byte>` of `plaintext.size() + nonce_size + tag_size` bytes: the nonce, the ciphertext, the tag.

## Complexity

Linear in the sizes of `plaintext` and `aad`.

## Exceptions

- `length_error` when `plaintext` is longer than `max_plaintext_size`.
- `logic_error` when the object was moved from and holds no key.

Nothing is drawn or sealed then.

## Notes

The nonce comes from [random::fill](../random/fill.md), the system's generator (`getentropy`); a failure of the
generator ends the program, as Go's `crypto/rand` does.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::xchacha20_poly1305 aead(vector<byte>(32));
    string text = "attack at dawn";
    string header = "to: hq";
    auto a = aead.seal_random(text, header);
    auto b = aead.seal_random(text, header);
    println("{} bytes, the same nonce twice: {}", a.size(), a.as_slice(0, 24) == b.as_slice(0, 24));
    println("{}", string(aead.open_random(b, header)));
}
```

Output:

```text
54 bytes, the same nonce twice: false
attack at dawn
```

## See also

- [open_random](open_random.md): opens what seal_random made
- [seal](../mixin/aead/seal.md): under a nonce the program gives
- [sgcl::crypto::xchacha20_poly1305](../xchacha20_poly1305.md)
