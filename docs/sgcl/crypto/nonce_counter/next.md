[sgcl](../../README.md) › [crypto](../README.md) › [nonce_counter](README.md)

# sgcl::crypto::nonce_counter::next

```cpp
array<byte, 12> next();
```

The next nonce: the counter's value, 96 bits written big-endian, after which the counter goes up by one. After the
last of the 2^96 nonces, all twelve bytes 0xff, the counter is spent; it does not wrap to zero.

## Parameters

None.

## Return value

The nonce, `nonce_size` bytes.

## Complexity

Constant.

## Exceptions

`out_of_range` when every nonce has been given, or the counter was moved from: the key must be replaced. The
counter stays spent.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // Across the 64th bit, and to the end
    array<byte, 12> start;
    for (int i : range(12)) {
        start[i] = byte(i < 4 ? 0x00 : 0xff);
    }
    crypto::nonce_counter nonces(start);
    println("{}", encoding::hex::encode(nonces.next()));
    println("{}", encoding::hex::encode(nonces.next()));

    array<byte, 12> last;
    last.fill(byte(0xff));
    crypto::nonce_counter ending(last);
    println("{}", encoding::hex::encode(ending.next()));
    try {
        ending.next();
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
00000000ffffffffffffffff
000000010000000000000000
ffffffffffffffffffffffff
sgcl::crypto::nonce_counter: every nonce has been used (or the counter was moved from)
```

## See also

- [(constructor)](nonce_counter.md): a counter from zero or from a given nonce on
- [aes_gcm](../aes_gcm/README.md), [chacha20_poly1305](../chacha20_poly1305/README.md): the AEADs that take its nonces
- [sgcl::crypto::nonce_counter](README.md)
