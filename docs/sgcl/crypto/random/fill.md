[sgcl](../../README.md) › [crypto](../README.md) › [random](../random.md)

# sgcl::crypto::random::fill

```cpp
void fill(const slice<byte>& out) noexcept;
```

Fills `out` with random bytes from the thread's generator, with no allocation: a stack array, a key's own storage, a
slice of a vector, the bytes of a [secret_bytes](../secret_bytes.md). Up to 1 KiB comes from the generator's buffer;
a larger request runs ChaCha20 under a one-time key of its own, straight into `out`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer filled |

## Return value

None.

## Complexity

Linear in the size of `out`.

## Exceptions

None. A system that gives no random bytes ends the program, with a line on stderr.

## Notes

A buffer of the program's own that holds a key is the program's to clear when the key is done, with
[secure_zero](../secure_zero.md); a [secret_bytes](../secret_bytes.md) clears itself.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <array>

using namespace sgcl;

int main() {
    std::array<byte, 8> id;
    crypto::random::fill(id);
    println("{}", encoding::hex::encode(id));

    std::array<byte, 32> key;  // on the stack
    crypto::random::fill(key);
    crypto::aes_gcm aead(key);
    // ... the key used ...
    crypto::secure_zero(key);
}
```

Sample output:

```text
64c2fa5e90f48920
```

## See also

- [secret](secret.md): random bytes as a secret
- [bytes](bytes.md): random bytes in a new vector
- [sgcl::crypto::random](../random.md)
