[sgcl](../../README.md) › [crypto](../README.md) › [shake256](../shake256.md)

# sgcl::crypto::shake256::read_to

```cpp
void read_to(const slice<byte>& out) noexcept;
```

Writes the next `out.size()` bytes of the output into `out`, with no allocation: [read](read.md) into a buffer of the
caller's — a stack array, a key's own storage. The first read pads the input and closes it, and every read goes on
where the last one stopped, whichever of the two forms made it. A buffer that holds a key is the caller's to clear
([secure_zero](../secure_zero.md)).

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer to fill: an `array<byte, N>`, a `byte` array, a `vector<byte>`, a slice of one |

## Return value

None.

## Complexity

Linear in `out.size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::shake256 x;
    x.update("abc");
    byte key[32];
    byte iv[32];
    x.read_to(key);
    x.read_to(iv);
    println(encoding::hex::encode(key));
    println(encoding::hex::encode(iv));
    crypto::secure_zero(key);
}
```

Output:

```text
483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739
d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4
```

## See also

- [read](read.md): the bytes as a `secret_bytes`
- [secure_zero](../secure_zero.md): clears the buffer after use
- [sgcl::crypto::shake256](../shake256.md)
