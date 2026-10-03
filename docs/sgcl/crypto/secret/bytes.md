[sgcl](../../README.md) › [crypto](../README.md) › [secret](README.md)

# sgcl::crypto::secret\<N\>::bytes, operator slice\<const byte\>

```cpp
slice<const byte> bytes() const noexcept;       // (1)
operator slice<const byte>() const noexcept;    // (2)
```

Returns the `N` bytes as a slice without an owner over the object's own memory, valid while the secret lives and
not moved from. Nothing is copied.

- (2) Implicit, so that a secret goes wherever the module takes bytes: `hkdf_sha256::derive(salt, shared, info, 32)`,
  `hmac_sha256(key)`, [constant_time::equal](../constant_time/equal.md).

## Parameters

None.

## Return value

A slice of `N` bytes.

## Complexity

Constant.

## Exceptions

None.

## Notes

The slice is the secret's own place; what the program copies out of it (into a `vector`, a string) is a copy the
secret does not clear.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Alice
    auto given =
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto alice = crypto::x25519::private_key::from_bytes(given);

    crypto::secret<32> key = alice->bytes();
    println("{} {}", key.bytes().size(), crypto::constant_time::equal(key, given));

    auto tag = crypto::hmac_sha256::of("a message", key);  // the key read in place
    println("{}", tag.size());
}
```

Output:

```text
32 true
32
```

## See also

- [operator==](operator_cmp.md): the comparison in constant time
- [sgcl::crypto::secret\<N\>](README.md)
