[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::parse

```cpp
static expected<jws, error> parse(const string& text) noexcept;
```

A JWS read, nothing verified: the compact serialization, or JSON (white space before `{`), flattened or general. Each
part is strict base64url (no padding, no other character), each protected header a JSON object with an `alg`. What
it reads is to choose a key by ([kid](kid.md), [header](header.md)), never to act on before [verify](verify.md).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JWS |

## Return value

The JWS, or an error: `errc::malformed` for anything that is not one (a compact form of other than three parts, a
part that is not base64url, a header that is not a JSON object or has no `alg`, a JSON form without its members or
with a member in both headers), `errc::unsupported` for a header with `crit` or `b64` false.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto j = crypto::jose::jws::parse("eyJhbGciOiJFZERTQSJ9.RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc."
        "hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg");
    println("{} {}", j->header().to_string(), string(j->unverified_payload()));
    println("{}", crypto::jose::jws::parse("a.b").error().message());
}
```

Output:

```text
{"alg":"EdDSA"} Example of Ed25519 signing
sgcl::crypto::jose: JWS: a compact JWS is three parts
```

## See also

- [verify](verify.md)
- [sgcl::crypto::jose::jws](README.md)
