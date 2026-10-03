[sgcl](../../README.md) › [hash](../README.md) › [xxh3_128](README.md)

# sgcl::hash::xxh3_128::value

```cpp
array<byte, 16> value() const noexcept;
```

The hash of the bytes hashed so far, sixteen bytes, the high half first, the canonical form `xxhsum -H128` prints;
the same as [digest](digest.md). It ends nothing: the hasher works the result out on a copy of its lanes, and
[update](update.md) may go on after it.

## Parameters

None.

## Return value

The hash, an `array<byte, 16>`.

## Complexity

Constant: the bytes still buffered, at most four stripes, and the merge of the lanes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::xxh3_128 h;
    h.update("hello");
    println("{}", encoding::hex::encode(h.value()));
    h.update(", world");
    println("{}", encoding::hex::encode(h.value()));
    println("{}", h.value() == hash::xxh3_128::of("hello, world"));
}
```

Output:

```text
b5e9c1ad071b3e7fc779cfaa5e523818
11c83d9c1ee368164c0abe17b55db69c
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::xxh3_128](README.md)
