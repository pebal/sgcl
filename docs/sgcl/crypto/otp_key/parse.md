[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::parse

```cpp
static expected<otp_key, error> parse(const string& uri) noexcept;
```

The key of an otpauth:// URI of Google Authenticator's Key Uri Format: the type `totp` or `hotp`, the label
`issuer:account`, and the parameters `secret` (base32, required), `issuer`, `algorithm` (`SHA1`, `SHA256`, `SHA512`),
`digits` (6 to 10), `period` (TOTP) and `counter` (HOTP, required). The scheme, the type, the names of the parameters,
the algorithm and the secret's base32 are read in any case; the secret's padding may be left out; parameters the
format does not name (`image`) are skipped; the issuer parameter says where the label's issuer ends, a colon (or
`%3A`) where there is none.

## Parameters

| Parameter | Description |
|---|---|
| `uri` | the URI, as text |

## Return value

The key, or an [error](../error/README.md): `errc::malformed` for a text that is not such a URI — another scheme, no
label, a bad percent-encoding, no secret or one that is not base32, a number that is not one, an HOTP key without
its counter — and `errc::unsupported` for another type, another algorithm, or digits outside 6 to 10.

## Complexity

Linear in the length of `uri`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string uri = "otpauth://totp/ACME%20Co:john@example.com?secret=JBSWY3DPEHPK3PXP&issuer=ACME%20Co&digits=8";
    auto key = crypto::otp_key::parse(uri);
    println("{} | {} | {}", key->issuer, key->account, key->options.digits);
    println("{}", crypto::otp_key::parse(string("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&algorithm=MD5")).error().message());
}
```

Output:

```text
ACME Co | john@example.com | 8
sgcl::crypto::otp_key: an algorithm other than SHA1, SHA256 and SHA512
```

## See also

- [to_string](to_string.md): the other way
- [(constructor)](otp_key.md): a URI of the program's own, thrown when wrong
- [sgcl::crypto::otp_key](README.md)
