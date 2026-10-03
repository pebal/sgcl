[sgcl](../../README.md) › [hash](../README.md) › [fnv128a](README.md)

# sgcl::hash::fnv128a::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: the offset basis, `0x6c62272e07bb014262b821756295c58d`. A hasher made by
[resume](resume.md) goes back to it too, not to the value it was resumed from. Go's `h.Reset()`.

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
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::fnv128a h;
    h.update("first");
    println("{}", encoding::hex::encode(h.value()));
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::fnv128a::of("second"));
}
```

Output:

```text
2a669da30583d94f708142c18d7ceb31
true
```

## See also

- [(constructor)](fnv128a.md): a hasher as it is made
- [sgcl::hash::fnv128a](README.md)
