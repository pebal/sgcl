[sgcl](../../README.md) › [hash](../README.md) › [fnv64a](../fnv64a.md)

# sgcl::hash::fnv64a::reset

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
    hash::fnv64a h;
    h.update("first");
    println("{:016x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::fnv64a::of("second"));
}
```

Output:

```text
89d7ed7f996f1d41
true
```

## See also

- [(constructor)](fnv64a.md): a hasher as it is made
- [sgcl::hash::fnv64a](../fnv64a.md)
