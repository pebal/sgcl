[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::verify_ip

```cpp
/*(1)*/ [[nodiscard]] expected<void, error> verify_ip(const slice<const byte>& ip) const;
/*(2)*/ template<class T>
        requires std::is_convertible_v<const T&, std::string_view>
        expected<void, error> verify_ip(const T& text) const = delete;
```

Checks that the certificate is for an IP address.

1. `ip` is 4 or 16 bytes in network order, compared byte for byte with the [ip_addresses](ip_addresses.md)
   (RFC 5280 §4.2.1.6); a 16-byte IPv4-mapped address (`::ffff:a.b.c.d`) is taken as its IPv4 address.
2. Deleted: a text, which bytes would take as its characters, is not an address: `"192.0.2.10"` would be ten bytes.
   The crypto module parses no address text and does not depend on `net`: `net::ip_address::bytes()` gives the
   bytes.

## Parameters

| Parameter | Description |
|---|---|
| `ip` | the address, 4 or 16 bytes in network order |

## Return value

Nothing, or a [crypto::error](../error.md) `errc::verification`, `reason::hostname_mismatch`.

## Complexity

Linear in the number of addresses.

## Exceptions

`invalid_argument` when `ip` is not 4 or 16 bytes: the address is the program's.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    const unsigned char v4[] = {127, 0, 0, 1};
    const unsigned char mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 127, 0, 0, 1};
    const unsigned char other[] = {192, 0, 2, 10};
    println("{} {}", cert.verify_ip(v4).has_value(), cert.verify_ip(mapped).has_value());
    println("{}", cert.verify_ip(other).error().message());
}
```

Output:

```text
true true
sgcl::crypto::x509: the certificate is not valid for the IP address asked
```

## See also

- [verify_options](../x509-verify_options.md): `ip`, the same check in a verification
- [verify_hostname](verify_hostname.md): a DNS name
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
