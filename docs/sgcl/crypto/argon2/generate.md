[sgcl](../../README.md) › [crypto](../README.md) › [argon2](README.md)

# sgcl::crypto::argon2::generate

```cpp
static string generate(const slice<const byte>& password);                      // (1)
static string generate(const slice<const byte>& password, const options& o);    // (2)
```

A hash of `password` for storage: 16 random bytes of salt, 32 bytes of Argon2 with the options, written as a PHC
string, `$argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>`, the salt and the hash in base64 without padding. The string
holds the variant, the version, the costs and the salt, so [verify](verify.md) needs nothing else but the password;
two strings of one password differ by their salt. The format is the reference implementation's, which Python's
`argon2-cffi`, PHP's `password_hash` and libsodium read and write.

A secret in `o` (a pepper) is hashed in and is not in the string: [verify](verify.md) must be given it again.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password, bytes or text |
| `o` | the variant, the costs and the secret ([options](../argon2-options.md); (1) the defaults) |

## Return value

The PHC string.

## Complexity

As [derive](derive.md).

## Exceptions

`std::invalid_argument` when `o.associated_data` is not empty (a PHC string has no place for it), and when the
options are out of their ranges, as for [derive](derive.md).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", crypto::argon2::generate("hunter2"));
    println("{}", crypto::argon2::generate("hunter2", {.memory = 19456, .iterations = 2, .parallelism = 1}));
}
```

Sample output:

```text
$argon2id$v=19$m=65536,t=3,p=4$MM+yHPs0CjbN6GkhnJFuRw$V1kQqd/JxSLX2v4q1u1sKws2/GnxqJSFH3//a3XRHpE
$argon2id$v=19$m=19456,t=2,p=1$7Kq2QbnY0Jx1CkCsT9q0lA$3dJ3Yv3Kc5j9e2l4K1j7Vq3Yp0n2c5ZkqHq7r2m1PzE
```

## See also

- [verify](verify.md): checks a password against the string
- [options](../argon2-options.md): the costs
- [sgcl::crypto::argon2](README.md)
