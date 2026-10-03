[sgcl](../../README.md) › [crypto](../README.md) › [hmac](README.md)

# sgcl::crypto::hmac\<H\>::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept;
```

Checks whether `tag` is the tag of the message so far: what a received tag is checked with, never `==`. The two are
compared in constant time ([constant_time](../constant_time/README.md)), so the time a check takes does not tell an attacker
how many leading bytes of a forgery were right; the tag computed for the comparison is zeroed before it returns. A tag
of another length is false. The hmac goes on, as after [value](value.md).

## Parameters

| Parameter | Description |
|---|---|
| `tag` | the tag received |

## Return value

`true` when `tag` is the tag of the message, `false` otherwise. `[[nodiscard]]`: a check whose result is dropped was
never made, and the compiler says so.

## Complexity

Constant: the tag of the message computed, and compared with `tag` in a time that depends only on the length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231, test case 2: the tag as it came with the message
    vector<byte> tag = encoding::hex::decode(
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843").value();
    crypto::hmac_sha256 mac("Jefe");
    mac.update("what do ya want for nothing?");
    println("{}", mac.verify(tag));

    vector<byte> cut(tag.begin(), tag.begin() + 16);  // a tag cut short
    println("{}", mac.verify(cut));
    tag[31] ^= byte(1);
    println("{}", mac.verify(tag));
}
```

Output:

```text
true
false
false
```

## See also

- [value](value.md): the tag itself
- [constant_time](../constant_time/README.md): the comparison it makes
- [sgcl::crypto::hmac\<H\>](README.md)
