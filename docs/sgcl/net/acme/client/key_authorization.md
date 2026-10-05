[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::key_authorization

```cpp
string key_authorization(const string& token) const;
```

The key authorization of a challenge's token (RFC 8555 §8.1): the token, a dot, the thumbprint of the client's
[key](key.md). The body http-01 serves at [http01_path](http01_path.md).

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
    net::acme::client acme("https://ca.example/directory", key);
    println("{}", acme.key_authorization("token-1") == "token-1." + key.thumbprint());
}
```

Output:

```text
true
```

## See also

- [http01_path](http01_path.md)
- [account_key::key_authorization](../account_key/key_authorization.md)
- [sgcl::net::acme::client](README.md)
