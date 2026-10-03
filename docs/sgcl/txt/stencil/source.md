[sgcl](../../README.md) › [txt](../README.md) › [stencil](README.md)

# sgcl::txt::stencil::source

```cpp
const string& source() const noexcept;
```

The source the template was read from, kept rather than copied: every run of literal text in a page is written out
of it. Empty for an empty template.

## Parameters

None.

## Return value

The source.

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
    string text = "Dear {{ name }},";
    txt::stencil letter(text);
    println("{} characters, shared: {}", letter.source().size(),
            letter.source().data() == text.data());
    return 0;
}
```

Output:

```text
16 characters, shared: true
```

## See also

- [sgcl::txt::stencil](README.md)
