[sgcl](../../README.md) › [txt](../README.md) › [format_pattern](../format_pattern.md)

# sgcl::txt::format_pattern\<A...\>::view

```cpp
constexpr std::string_view view() const noexcept;
```

The text the pattern was made of, as it was written: valid while that text is, which for a literal is the whole run
of the program.

## Parameters

None.

## Return value

The text of the pattern.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr txt::format_pattern<int> left("{} left");
    println("{:?}, {} characters, gives {:?}", left.view(), left.view().size(),
            txt::format(left, 3));
    return 0;
}
```

Output:

```text
"{} left", 7 characters, gives "3 left"
```

## See also

- [sgcl::txt::format_pattern](../format_pattern.md)
