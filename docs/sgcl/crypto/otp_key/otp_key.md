[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::otp_key

```cpp
otp_key();                              // (1)
explicit otp_key(const string& uri);    // (2)
otp_key(otp_key&& other) noexcept;      // (3)
otp_key(const otp_key&) = delete;       // (4)
```

1. An empty TOTP key with the default options, for the program to fill.
2. The key a URI in the program spells: what [parse](parse.md) reads, or `bad_expected_access<crypto::error>` with
   `parse`'s error. A URI from outside the program (a scanned QR code, a setting) is parsed, and its error is a value.
3. Takes the key of `other` over; `other`'s secret is left empty.
4. The secret is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `uri` | an otpauth:// URI |
| `other` | the key taken over |

## Complexity

Linear in the length of `uri`.

## Exceptions

- (2) `bad_expected_access<crypto::error>` when `uri` is not a key's URI; its `error()` is `parse`'s, of the code
  `errc::malformed` or `errc::unsupported`.
- (1), (3) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key("otpauth://hotp/Bank:12345?secret=GEZDGNBVGY3TQOJQ&counter=9");
    println("{} {} {}", key.issuer, key.account, key.counter);
    try {
        crypto::otp_key broken("otpauth://totp/Bank:12345");
    } catch (const bad_expected_access<crypto::error>& e) {
        println(e.error().message());
    }
}
```

Output:

```text
Bank 12345 9
sgcl::crypto::otp_key: no secret
```

## See also

- [parse](parse.md): a URI from outside, into an `expected`
- [sgcl::crypto::otp_key](README.md)
