[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::next_uint64

```cpp
uint64_t next_uint64() noexcept;
```

The next word of the stream, all 64 bits: what Go's `Uint64` draws from the same key. Every other method of the
generator is computed from these words.

## Parameters

None.

## Return value

The word drawn.

## Complexity

Constant: a word of a buffer of 32, four ChaCha8 blocks made when the buffer runs out.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    println("{:#018x}", r.next_uint64());
    println("{:#018x}", r.next_uint64());
}
```

Output:

```text
0xda7829d8b81f3022
0x349f961456b007f0
```

## See also

- [operator()](operator_call.md): the same word, for the distributions of `<random>`
- [next_bytes](next_bytes.md): the words as bytes
- [sgcl::math::random](README.md)
