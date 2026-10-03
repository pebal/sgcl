[sgcl](../../README.md) › [crypto](../README.md) › [constant_time](README.md)

# sgcl::crypto::constant_time::equal

```cpp
[[nodiscard]] bool equal(const slice<const byte>& a, const slice<const byte>& b) noexcept;
```

Checks whether `a` and `b` hold the same bytes, Go's `subtle.ConstantTimeCompare`. Every byte of both is read,
eight at a time, whatever they hold: the differences are ORed together and the result is made from the sum with no
branch on it, and an empty asm keeps the compiler from turning the loop back into an early exit at the first byte
that differs. The lengths are not secret: two slices of different sizes are unequal at once.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the bytes compared |

## Return value

`true` when the slices are of one size and hold the same bytes.

## Complexity

Linear in the size when the sizes are equal, in a time that depends on the size only; constant otherwise.

## Exceptions

None.

## Notes

`[[nodiscard]]`: a comparison whose result is dropped is a check that was never made, and the compiler warns.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231's test case 2
    auto tag = crypto::hmac_sha256::of("what do ya want for nothing?", "Jefe");
    vector<byte> published = encoding::hex::decode(
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    println("{}", crypto::constant_time::equal(tag, published));

    vector<byte> forged = published;
    forged.back() ^= byte{1};
    println("{}", crypto::constant_time::equal(tag, forged));
    println("{}", crypto::constant_time::equal(tag, published.as_slice().subslice(0, 16)));
}
```

Output:

```text
true
false
false
```

## See also

- [hmac](../hmac/README.md): a tag under a key, and its `verify`
- [sgcl::crypto::constant_time](README.md)
