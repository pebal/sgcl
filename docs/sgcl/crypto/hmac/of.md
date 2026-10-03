[sgcl](../../README.md) › [crypto](../README.md) › [hmac](../hmac.md)

# sgcl::crypto::hmac\<H\>::of

```cpp
static array<byte, digest_size> of(const slice<const byte>& data,
                                   const slice<const byte>& key) noexcept;
```

The tag of `data` under `key` in one call: an hmac made with the key, updated with the data, asked its value. The
data comes first and the key after it, as every keyed type of the hash module has it (`siphash::of(data, key)`); the
constructor takes the key alone. Both are bytes or text, which the slice takes both. The hmac made inside zeroes its
states when the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the message, bytes or text |
| `key` | the secret key, bytes or text, of any length |

## Return value

The tag, `H::digest_size` bytes.

## Complexity

Linear in the lengths of `data` and `key`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231, test case 2
    auto tag = crypto::hmac_sha512::of("what do ya want for nothing?", "Jefe");
    println(encoding::hex::encode(tag));
}
```

Output:

```text
164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737
```

## See also

- [(constructor)](hmac.md): the key alone, for a message in pieces
- [verify](verify.md): checks a received tag
- [sgcl::crypto::hmac\<H\>](../hmac.md)
