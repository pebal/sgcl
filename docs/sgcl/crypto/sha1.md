# sgcl::crypto::sha1

```cpp
#include "sgcl/crypto/sha1.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    class sha1;   // SHA-1 (FIPS 180-4)    value(): array<byte, 20>
}
```

SHA-1, Go's `crypto/sha1`: a digest of 20 bytes that has been broken for collisions since 2017 (SHAttered: two PDFs with one digest) and is kept for what still names it — Git's object names, HMAC-SHA-1 in TOTP (RFC 6238) and older protocols, PBKDF2-HMAC-SHA1 of RFC 6070. A new design takes [`sha256`](sha256.md) or better; a signature or a certificate over SHA-1 is refused by the module's x509.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The shape of a hasher** of the [hash module](../hash/README.md), as [`sha256`](sha256.md) has it; a copy is a branch; about 100 bytes, trivially copyable.
- **HMAC-SHA-1 is not broken** by SHA-1's collisions: an HMAC needs the digest to be a pseudorandom function, which is another property. What cannot be trusted is a digest standing alone for its data where someone could have prepared two.

## Paths

On arm64 the SHA-1 instructions of ARMv8 (`SHA1C`, `SHA1P`, `SHA1M`, `SHA1H`, `SHA1SU0`, `SHA1SU1`), chosen as for [`sha256`](sha256.md) (`FEAT_SHA1`, `HWCAP_SHA1`); plain C++ elsewhere and with `SGCL_CRYPTO_PORTABLE`.

## Members

```cpp
static constexpr size_t digest_size = 20;
static constexpr size_t block_size = 64;

sha1() noexcept;
void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
array<byte, 20> value() const noexcept;
array<byte, 20> digest() const noexcept;
void reset() noexcept;

static array<byte, 20> of(/* bytes or text */) noexcept;
expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
static expected<array<byte, 20>, io::error> of_file(const string& path);  static async::task<expected<array<byte, 20>, io::error>> async_of_file(const string& path);   // of() of the whole file, through copy_from
```

## Example

The program prints what `git hash-object` prints for a file holding `hello`.

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

#include <array>

using namespace sgcl;

int main() {
    // Git's name of a blob: SHA-1 of "blob <size>", a zero byte and the contents
    string contents = "hello\n";
    crypto::sha1 h;
    h.update(txt::format("blob {}", contents.size()));
    h.update(std::array{byte(0)});
    h.update(contents);
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
ce013625030ba8dba906f756967f9e9ca394464a
```

## See also

[`sha256`](sha256.md), [`sha512`](sha512.md), [`sha3`](sha3.md); [`hmac`](hmac.md); [`pbkdf2`](pbkdf2.md).
