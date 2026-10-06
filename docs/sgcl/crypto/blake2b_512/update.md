[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes the bytes of `data` in, after everything hashed before: a message fed in pieces of any length has the digest of
the whole. The slice takes what bytes come in: a `vector<byte>`, an `array<byte, N>`, a `secret_bytes`, a slice of
them. Whole blocks are compressed straight from `data`; the last block of what has come so far waits in the hasher's
buffer, since BLAKE2 marks the last block of a message and only the next call or `value()` tells which one it is.

The text forms — a `string`, a text slice, a literal, a C string, a `std::string_view` — and a `std::span` of bytes
are the mixin's ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)), each hashing the UTF-8 bytes where they lie.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to hash |

## Return value

None.

## Complexity

Linear in `data.size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a million times "a", a thousand at a time
    crypto::blake2b_512 h;
    string thousand(1000, 'a');
    for (int i : range(1000)) {
        h.update(thousand);
    }
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
98fb3efb7206fd19ebf69b6f312cf7b64e3b94dbe1a17107913975a793f177e1d077609d7fba363cbba00d05f7aa4e4fa8715d6428104c0a75643b0ff3fd3eaf
```

## See also

- [value](value.md): the digest of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::blake2b_512](README.md)
