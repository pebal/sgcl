[sgcl](../../README.md) › [hash](../README.md) › [fnv64](../fnv64.md)

# sgcl::hash::fnv64::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: the offset basis, `0xcbf29ce484222325`. A hasher made by [resume](resume.md)
goes back to it too, not to the value it was resumed from. Go's `h.Reset()`.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::fnv64 h;
    h.update("first");
    println("{:016x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::fnv64::of("second"));
}
```

Output:

```text
c0de9a9b8ec0e479
true
```

## See also

- [(constructor)](fnv64.md): a hasher as it is made
- [sgcl::hash::fnv64](../fnv64.md)
