[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::client

```cpp
client(const string& directory_url, const account_key& key);                      // (1)
client(const string& directory_url, const account_key& key, const options& o);    // (2)
client(const client& other) = default;                                            // (3)
```

1. A client of the CA whose directory is at `directory_url`, signing with `key`, with the default
   [options](../client-options.md). Nothing is sent until the first call.
2. The same with the options `o`.
3. The same client: a copy of the handle, its directory, account and nonces shared.

## Parameters

| Parameter | Description |
|---|---|
| `directory_url` | the URL of the CA's directory: [lets_encrypt_url](../README.md#objects-and-types), a test server's |
| `key` | the account's key (its copies are the same key) |
| `o` | how the client talks to the CA |
| `other` | the client to share |

## Complexity

Constant.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::account_key key;
    net::acme::client acme(ca.directory_url(), key);
    net::acme::client same = acme;
    acme.register_account({.terms_agreed = true});
    println("{} {}", same.account_url() == acme.account_url(),
            acme.directory_url() == ca.directory_url());
}
```

Output:

```text
true true
```

## See also

- [options](../client-options.md)
- [sgcl::net::acme::client](README.md)
