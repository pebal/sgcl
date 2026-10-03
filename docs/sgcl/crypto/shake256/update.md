[sgcl](../../README.md) › [crypto](../README.md) › [shake256](README.md)

# sgcl::crypto::shake256::update

```cpp
void update(const slice<const byte>& data);
```

Absorbs the bytes of `data` into the sponge, after everything absorbed before: input fed in pieces of any length
gives the output of the whole. The slice takes bytes and text alike — a `vector<byte>`, an `array<byte, N>`, a
`string`, a literal (to its first NUL), a `std::string_view` — the forms a hasher's `update` takes. The input is open
until the first [read](read.md) or [read_to](read_to.md), which pads it and closes it.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to absorb |

## Return value

None.

## Complexity

Linear in `data.size()`.

## Exceptions

`std::invalid_argument` when the input is closed: an `update` after a read, before a [reset](reset.md). Nothing is
absorbed, and the sponge reads on as before.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::shake256 x;
    x.update("ab");
    x.update("c");
    println(encoding::hex::encode(x.read(32)));
    try {
        x.update("d");
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739
sgcl::crypto::shake: update after read
```

## See also

- [read](read.md): the output, which closes the input
- [reset](reset.md): the input open again
- [sgcl::crypto::shake256](README.md)
