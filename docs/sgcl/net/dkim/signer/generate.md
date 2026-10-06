[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::generate

```cpp
static signer generate(const string& domain, const string& selector,
                       dkim::algorithm a = dkim::algorithm::rsa_sha256);
```

A signer of a new key: RSA of 2048 bits (what RFC 8301 asks a signer to use at least) or Ed25519. Its
[record](record.md) is what the domain publishes, its [private_key_pem](private_key_pem.md) what is kept for the
next run.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | the signing domain, `d=` |
| `selector` | the selector, `s=` |
| `a` | the [algorithm](../algorithm.md) of the key |

## Return value

The signer.

## Complexity

An RSA key: two primes of 1024 bits searched, some tens of milliseconds; an Ed25519 key: one scalar
multiplication.

## Exceptions

`bad_expected_access<io::error>` of `net::errc::invalid_address` for a domain or a selector that is no name.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "ed", net::dkim::algorithm::ed25519_sha256);
    println("{}", s.record().size());
}
```

Output:

```text
66
```

## See also

- [algorithm](../algorithm.md)
- [signer](README.md)
