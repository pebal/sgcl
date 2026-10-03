[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::length

```cpp
size_type length() const noexcept;
```

Returns the number of characters, `size()` of the class that carries the mixin: units of `CharT`, so bytes in a UTF-8
text. The number of code points is [rune_count](rune_count.md).

## Parameters

None.

## Return value

The number of units of `CharT`.

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
    string s = "żółw";
    println("{} {} {}", s.length(), s.size(), s.rune_count());
    println("{}", s.as_slice(2).length());
}
```

Output:

```text
7 7 4
5
```

## See also

- [rune_count](rune_count.md): the number of code points
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)
