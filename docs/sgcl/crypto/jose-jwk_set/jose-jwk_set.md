[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::jwk_set

```cpp
jwk_set() noexcept;                          // (1)
jwk_set(std::initializer_list<jwk> keys);    // (2)
explicit jwk_set(const string& text);        // (3)
```

1. An empty set.
2. A set of the keys, in their order.
3. The set of a JWK Set's JSON the program spells: [parse](parse.md)'s value.

## Parameters

| Parameter | Description |
|---|---|
| `keys` | the keys |
| `text` | the JSON of a JWK Set |

## Complexity

- (1) Constant.
- (2) Linear in the number of keys.
- (3) Linear in the text's length.

## Exceptions

- (1–2) None.
- (3) `bad_expected_access<crypto::error>` with [parse](parse.md)'s error for a text that does not read.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk_set none;
    crypto::jose::jwk_set two{crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa),
                              crypto::jose::jwk::generate(crypto::jose::algorithm::es256)};
    println("{} {}", none.size(), two.size());
}
```

Output:

```text
0 2
```

## See also

- [parse](parse.md): a set of a JWK Set's JSON
- [sgcl::crypto::jose::jwk_set](README.md)
