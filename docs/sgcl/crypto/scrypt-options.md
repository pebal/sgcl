[sgcl](../README.md) › [crypto](README.md) › [scrypt](scrypt/README.md)

# sgcl::crypto::scrypt::options

```cpp
#include "sgcl/crypto/scrypt.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class scrypt {
    public:
        struct options {
            uint32_t cost = 32768;
            uint32_t block_size = 8;
            uint32_t parallelism = 1;
        };
    };
}
```

The costs of [scrypt](scrypt/README.md), RFC 7914's `N`, `r` and `p`. The memory is `128 · r · N` bytes and the time
grows with `N · r · p`. The defaults are Go's documented choice for an interactive login, 32 MiB; Go's for a file's
encryption key is `N = 1048576`, 1 GiB.

## Member objects

| Member | Description |
|---|---|
| `cost` | `N`, the blocks of ROMix's memory: a power of two above 1; 32768 by default |
| `block_size` | `r`, a block of 128 `r` bytes; 8 by default |
| `parallelism` | `p`, the blocks ROMix runs over, one after the other; 1 by default. `r · p` stays under 2^30 |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7914's second vector, its first 32 bytes
    crypto::scrypt::options o{.cost = 1024, .block_size = 8, .parallelism = 16};
    println(encoding::hex::encode(crypto::scrypt::derive("password", "NaCl", 32, o)));
}
```

Output:

```text
fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b373162
```

## See also

- [scrypt](scrypt/README.md): what takes them
- [The module](README.md)
