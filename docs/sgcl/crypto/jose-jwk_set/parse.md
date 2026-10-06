[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::parse

```cpp
static expected<jwk_set, error> parse(const string& text) noexcept;
static expected<jwk_set, error> parse(const secret_bytes& text) noexcept;
```

The keys of a JWK Set's JSON, `{"keys":[…]}`, each read as [jwk::parse](../jose-jwk/parse.md) reads one (private
members straight into plain memory). A key of a `kty` or a curve the module does not have is passed over (RFC 7517
§5); the other members of the object are ignored.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JSON: a string, or a private set's in a secret_bytes |

## Return value

The set, or an error: `errc::malformed` for text that is not one JSON object with an array `keys`, and the error of
a key that does not read for any other reason than its kind.

## Complexity

Linear in the length of the text, and the check of each key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto set = crypto::jose::jwk_set::parse(R"({"keys":[
        {"kty":"OKP","crv":"Ed25519","kid":"a","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"},
        {"kty":"OKP","crv":"Ed448","kid":"b","x":"AA"}]})");
    println("{} {}", set->size(), (*set)[0].kid());
}
```

Output:

```text
1 a
```

## See also

- [to_json](to_json.md): the other way
- [sgcl::crypto::jose::jwk_set](README.md)
