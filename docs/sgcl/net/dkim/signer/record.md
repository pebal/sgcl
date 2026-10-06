[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::record

```cpp
string record() const;
```

The TXT record to publish at [record_name](record_name.md) (RFC 6376 §3.6.1): `v=DKIM1`, the key's type and the
public key in base64 — an RSA key's SubjectPublicKeyInfo, an Ed25519 key's 32 bytes (RFC 8463). A record of more
than 255 bytes (an RSA key's) is published as several strings of one TXT record, which DNS joins.

## Parameters

None.

## Return value

The record's text: `"v=DKIM1; k=rsa; p=MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA..."`.

## Complexity

Linear in the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::from_seed(crypto::sha256::of("the example's key"));
    net::dkim::signer s("example.com", "s1", key->to_pem());
    println("{}", s.record());
}
```

Output:

```text
v=DKIM1; k=ed25519; p=Z85EUS50tHh9IrAd8rkvYCvbCCU976TepgWn6oGCigc=
```

## See also

- [record_name](record_name.md), [private_key_pem](private_key_pem.md)
- [signer](README.md)
