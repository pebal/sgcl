[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::dns01_value

```cpp
string dns01_value(const string& token) const;
```

The value of dns-01's TXT record for a token (RFC 8555 §8.4): the SHA-256 of the
[key authorization](key_authorization.md), base64url, 43 characters, published as the TXT record
[dns01_name](dns01_name.md) before the challenge is [accept](accept.md)ed.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the challenge's token |

## Return value

The value.

## Complexity

Linear in the length of the token.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key;
    net::acme::client acme("https://ca.example/directory", key);
    string value = acme.dns01_value("token-1");
    auto digest = crypto::sha256::of(acme.key_authorization("token-1"));
    println("{} {}", value.size(), value == encoding::base64::raw_url.encode(digest));
}
```

Output:

```text
43 true
```

## See also

- [dns01_name](dns01_name.md)
- [sgcl::net::acme::client](README.md)
