[sgcl](../../README.md) › [txt](../README.md) › [runtime_pattern](README.md)

# sgcl::txt::runtime_pattern::view

```cpp
constexpr std::string_view view() const noexcept;
```

The characters of the pattern, a view over the string it keeps: valid while the pattern is.

## Parameters

None.

## Return value

The characters of the pattern.

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
    auto pattern = txt::runtime("{} left");
    println("{} characters: {}", pattern.view().size(), pattern.view());
    return 0;
}
```

Output:

```text
7 characters: {} left
```

## See also

- [text](text.md): the string itself
- [sgcl::txt::runtime_pattern](README.md)
