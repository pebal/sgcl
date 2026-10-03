[sgcl](../../README.md) › [crypto](../README.md) › [shake256](../shake256.md)

# sgcl::crypto::shake256::of

```cpp
static secret_bytes of(const slice<const byte>& data, size_t n) noexcept;
```

The first `n` bytes of the output over `data`, in one call: a sponge made, `data` absorbed and `n` bytes read, what
Go's `sha3.SumSHAKE256(data, n)` gives. `shake128::of` is the same over SHAKE128. The data is bytes or text, which
the slice takes both. The bytes are a [secret_bytes](../secret_bytes.md), as [read](read.md) gives them.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the input, bytes or text |
| `n` | the number of bytes of output, any |

## Return value

The first `n` bytes of the output.

## Complexity

Linear in `data.size()` and `n`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::shake128::of("abc", 32)));
    println(encoding::hex::encode(crypto::shake256::of("abc", 64)));
}
```

Output:

```text
5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8
483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4
```

## See also

- [read](read.md): the output read in pieces
- [sgcl::crypto::shake256](../shake256.md)
