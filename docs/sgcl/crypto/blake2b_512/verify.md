[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept;
```

Checks whether `tag` is the digest of the message so far — with a key, its MAC: what a received tag is checked with,
never `==`. The two are compared in constant time ([constant_time](../constant_time/README.md)), so the time a check
takes does not tell an attacker how many leading bytes of a forgery were right; the digest computed for the
comparison is zeroed before it returns. A tag of another length is false. The hasher goes on, as after
[value](value.md).

## Parameters

| Parameter | Description |
|---|---|
| `tag` | the tag received |

## Return value

`true` when `tag` is the digest of the message, `false` otherwise. `[[nodiscard]]`: a check whose result is dropped
was never made, and the compiler says so.

## Complexity

Constant: the digest computed, and compared with `tag` in a time that depends only on the length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    vector<byte> tag = encoding::hex::decode("37d0934cac1b83cf742574ea32dd9481");
    crypto::blake2s_128 mac({.key = key});
    mac.update("message");
    println("{}", mac.verify(tag));
    tag[0] ^= byte(1);
    println("{}", mac.verify(tag));
}
```

Output:

```text
true
false
```

## See also

- [value](value.md): the tag itself
- [constant_time](../constant_time/README.md): the comparison it makes
- [sgcl::crypto::blake2b_512](README.md)
