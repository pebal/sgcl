[sgcl](../README.md) › [crypto](README.md) › [argon2](argon2/README.md)

# sgcl::crypto::argon2::variant

```cpp
#include "sgcl/crypto/argon2.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class argon2 {
    public:
        enum class variant : uint8_t {
            d = 0,
            i = 1,
            id = 2
        };
    };
}
```

The three variants of Argon2, the type `y` of RFC 9106 §3.2, which is also the value of each. They differ in how a
block chooses the block it is computed with: from the data, which an attacker who can watch the memory's access
pattern learns something from, or from a counter, which is weaker against trade-offs of time against memory. Argon2id
does both, and is the one RFC 9106 recommends.

| Value | Description |
|---|---|
| `d` | Argon2d: blocks chosen by the data: the strongest against trade-offs, open to side channels; for where no one can watch |
| `i` | Argon2i: blocks chosen by a counter alone: no side channel on the password, weaker against trade-offs |
| `id` | Argon2id: the first half of the first pass as Argon2i, the rest as Argon2d: the default and the recommended one |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::argon2::options o{.variant = crypto::argon2::variant::i, .memory = 65536, .iterations = 2,
                              .parallelism = 1};
    println(encoding::hex::encode(crypto::argon2::derive("password", "somesalt", 32, o)));
}
```

Output:

```text
c1628832147d9720c5bd1cfd61367078729f6dfb6f8fea9ff98158e0d7816ed0
```

## See also

- [argon2::options](argon2-options.md): where the variant is chosen
- [argon2](argon2/README.md)
- [The module](README.md)
