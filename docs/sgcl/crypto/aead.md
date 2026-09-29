# sgcl::crypto::mixin::aead

```cpp
#include "sgcl/crypto/mixin/aead.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto::mixin {
    template<class Derived> class aead;   // seal and open over Derived's two operations
}
```

**The implementation has not been through an independent cryptographic audit.**

The interface every authenticated cipher of the module shares — [`aes_gcm`](aes_gcm.md), [`chacha20_poly1305` and `xchacha20_poly1305`](chacha20_poly1305.md) — as Go's `cipher.AEAD`: **seal** encrypts a plaintext and appends a tag that authenticates it together with additional data sent in the clear; **open** checks the tag and gives the plaintext back, or an error when anything — the ciphertext, the tag, the nonce, the additional data or the key — is not what was sealed. Each call comes in two forms: into a new `vector<byte>`, or into a buffer the caller gives, with nothing allocated; each with and without additional data.

## Members

```cpp
vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext) const;
vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext, const slice<const byte>& aad) const;

[[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce, const slice<const byte>& sealed) const;   // the plaintext: the user's data
[[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce, const slice<const byte>& sealed, const slice<const byte>& aad) const;

size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& plaintext) const;
size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& plaintext, const slice<const byte>& aad) const;

[[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& sealed) const;
[[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& sealed, const slice<const byte>& aad) const;

// From the class
static constexpr size_t nonce_size;          // 12, or 24 for xchacha20_poly1305
static constexpr size_t tag_size = 16;
static constexpr size_t overhead = 16;       // what seal adds to the plaintext
static constexpr uint64_t max_plaintext_size;
```

- **Sealed** data is the ciphertext followed by the tag: `plaintext.size() + tag_size` bytes, as Go's `Seal` with a nil `dst` makes. The nonce is not in it; the program sends or stores the nonce beside it (or uses [`xchacha20_poly1305::seal_random`](chacha20_poly1305.md), which writes it in front).
- **`open`** gives `errc::authentication` (the message "message authentication failed") for every kind of mismatch, for sealed data shorter than a tag and for sealed data longer than `max_plaintext_size + tag_size` (which `seal` would have refused with `std::length_error`: a length the program chose is a broken contract, a length that came with the data is a forgery; Go panics on both), and says nothing more: which bytes were wrong is not an answer an attacker may have.
- **`seal_to`** writes `plaintext.size() + tag_size` bytes and returns that number; **`open_to`** writes `sealed.size() - tag_size` bytes and returns that number.
- The functions are `const`: one object seals and opens from any number of threads at once.

## Rules

- **The contract**, a broken one being an exception, as everywhere in the library: the nonce is exactly `nonce_size` bytes (`std::invalid_argument`); the output of `seal_to` holds the plaintext and the tag, the output of `open_to` the sealed data less the tag (`std::length_error`); a plaintext is at most `max_plaintext_size` bytes (`std::length_error`); an object moved from holds no key (`std::logic_error`).
- **In place.** `out` may be the input itself — the same first byte — and the work is done in place: `seal_to(buffer, nonce, buffer.as_slice(0, n))` with room for the tag after the text, `open_to(buffer, nonce, buffer)`. Any other overlap of `out` and the input is `std::invalid_argument`, as Go panics on it: a copy shifted by a few bytes would read what it has just written.
- **Nothing of a forgery is ever written.** `open` computes the tag over the ciphertext and compares it, in constant time ([`constant_time::equal`](constant_time.md)), *before* the first byte is decrypted; when the tag does not match, the bytes `open_to` would have written are set to zero and nothing else of `out` is touched. A program that ignores the error reads zeros, not the attacker's text and not the ciphertext; opened in place, the ciphertext is zeroed too. This costs a second pass over the data on open (the tag first, then the decryption), which one-pass implementations save by writing the plaintext first and wiping it on failure.
- **The plaintext of `open`** is the user's data, not key material: a managed `vector<byte>`, as `seal` gives the ciphertext, which the collector frees in its time and nobody zeroes. [`secret_bytes`](secret.md#secret_bytes) is for keys, derived secrets, private keys' exports and passwords. For a plaintext that must not stay in memory — a key unwrapped, a password — `open_to` into a buffer the program clears with [`secure_zero`](secure_zero.md) when done (a `secret_bytes` of the plaintext's length is one: `open_to(s.as_slice(), ...)`).
- **`[[nodiscard]]`** on `open` and `open_to`: a dropped result of a verification is a hole, so the compiler warns about it, which the library does not do elsewhere.

## See also

[The module](README.md); [`aes_gcm`](aes_gcm.md); [`chacha20_poly1305`, `xchacha20_poly1305`](chacha20_poly1305.md); [`nonce_counter`](nonce_counter.md); [`error`](error.md).
