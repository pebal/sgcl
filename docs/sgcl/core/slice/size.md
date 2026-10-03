[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::size

```cpp
size_type size() const noexcept;
```

The number of elements: `end() - begin()`. For a text slice, the number of code units, bytes for UTF-8; the number of
code points is `rune_count()` ([mixin::text](../mixin/text/README.md)).

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "żółw";
    string_slice s = text;
    println("{} {}", s.size(), s.rune_count());
}
```

Output:

```text
7 4
```

## See also

- [size_bytes](size_bytes.md): the size in bytes
- [empty](empty.md): checks whether the slice is empty
- [sgcl::slice\<T\>](README.md)
