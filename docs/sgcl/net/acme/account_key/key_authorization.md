[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::key_authorization

```cpp
string key_authorization(const string& token) const;
```

The key authorization of a challenge's token (RFC 8555 §8.1): the token, a dot, the [thumbprint](thumbprint.md). What
http-01 serves; dns-01's value and tls-alpn-01's certificate are made of its SHA-256.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the challenge's token |

## Return value

The key authorization.

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
    string ka = key.key_authorization("evaGxfADs6pSRb2LAv9IZf17Dt3juxGJ-PCt92wr-oA");
    println("{} {}", ka.size(), ka.view().substr(44) == key.thumbprint().view());
}
```

Output:

```text
87 true
```

## See also

- [client::key_authorization](../client/key_authorization.md)
- [sgcl::net::acme::account_key](README.md)
