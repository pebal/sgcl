[sgcl](../../README.md) › [hash](../README.md) › [xxh3_64](README.md)

# sgcl::hash::xxh3_64::value

```cpp
uint64_t value() const noexcept;
```

The hash of the bytes hashed so far. It ends nothing: the hasher works the result out on a copy of its lanes, and
[update](update.md) may go on after it.

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
    hash::xxh3_64 h;
    h.update("hello");
    println("{:016x}", h.value());
    h.update(", world");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::xxh3_64::of("hello, world"));
}
```

Output:

```text
9555e8555c62dcfd
302cd5fba73d006c
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::xxh3_64](README.md)
