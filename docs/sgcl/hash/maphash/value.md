[sgcl](../../README.md) › [hash](../README.md) › [maphash](../maphash.md)

# sgcl::hash::maphash::value

```cpp
uint64_t value() const noexcept;
```

The hash of the bytes hashed so far, for this process: Go's `h.Sum64()`. It ends nothing: the hasher works the
result out on a copy of its state, and [update](update.md) may go on after it.

## Parameters

None.

## Return value

The hash, a `uint64_t`.

## Complexity

Constant: the bytes still buffered, at most four stripes, and the merge of the lanes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::maphash h;
    h.update("hello");
    println("{:016x}", h.value());
    h.update(", world");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::maphash::of("hello, world"));
}
```

Sample output:

```text
b8c447af54b12c4a
2c924ee887eff451
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::maphash](../maphash.md)
