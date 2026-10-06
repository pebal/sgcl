[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::sign_json

```cpp
static string sign_json(const slice<const byte>& payload, const jwk& key);                           // (1)
static string sign_json(const slice<const byte>& payload, const jwk& key, const sign_options& o);    // (1)
static string sign_json(const slice<const byte>& payload, const jwk_set& keys);                      // (2)
static string sign_json(const slice<const byte>& payload, const jwk_set& keys,                       // (2)
                        const sign_options& o);
```

The JSON serialization of the payload signed (RFC 7515 §7.2), each signature as [sign](sign.md) makes it:

1. Flattened, one signature: `{"payload":…,"protected":…,"signature":…}`, what ACME sends.
2. General, a signature by every key of the set: `{"payload":…,"signatures":[{"protected":…,"signature":…},…]}`.

## Parameters

| Parameter | Description |
|---|---|
| `payload` | the payload, bytes or text |
| `key` | a private key, or an oct key |
| `keys` | the keys, each a private key or an oct key |
| `o` | the algorithm, for every key, and the header ([sign_options](../jose-sign_options.md)) |

## Return value

The JWS, compact JSON.

## Complexity

Linear in the length of the payload, and a signature by each key.

## Exceptions

`std::invalid_argument` as [sign](sign.md) has it, for a key that cannot sign, and for an empty set.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...", {.kid = "a"});
    println("{}", crypto::jose::jws::sign_json("hi", a));
    auto b = crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "b"});
    auto both = crypto::jose::jws::sign_json("hi", crypto::jose::jwk_set{a, b});
    println("{}", crypto::jose::jws::parse(both)->signature_count());
}
```

Output:

```text
{"payload":"aGk","protected":"eyJhbGciOiJIUzI1NiIsImtpZCI6ImEifQ","signature":"yjd41VjGNLNPCFZgnWWwTUUw6az9AsY2MpuCNHyzmrw"}
2
```

## See also

- [sign](sign.md): the compact serialization
- [parse](parse.md)
- [sgcl::crypto::jose::jws](README.md)
