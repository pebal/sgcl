[sgcl](../../README.md) › [crypto](../README.md) › [sha512](../sha512.md)

# sgcl::crypto::sha512::value

```cpp
array<byte, 64> value() const noexcept;   // sha512
array<byte, 48> value() const noexcept;   // sha384
array<byte, 32> value() const noexcept;   // sha512_256
```

The digest of everything hashed in so far. The padding and the last blocks are computed on a copy of the state, so the
hasher goes on: more `update` after `value()` gives the digest of the longer message, as Go's `Sum` leaves its hash
as it was. For `sha384` and `sha512_256` the digest is the first six or four of the eight words; the other words do
not stay: the buffer they were written to is zeroed before `value()` returns.

## Parameters

None.

## Return value

The digest: 64 bytes for `sha512`, 48 for `sha384`, 32 for `sha512_256`.

## Complexity

Constant: one or two compressions of the last block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::sha384 h;
    h.update("ab");
    auto ab = h.value();
    h.update("c");  // goes on after "ab"
    println(encoding::hex::encode(h.value()));
    println("{} {}", ab.size(), ab == crypto::sha384::of("ab"));
}
```

Output:

```text
cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
48 true
```

## See also

- [digest](digest.md): the same bytes, under the name every hasher has
- [update](update.md): hashes bytes in
- [sgcl::crypto::sha512](../sha512.md)
