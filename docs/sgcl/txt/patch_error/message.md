[sgcl](../../README.md) › [txt](../README.md) › [patch_error](README.md)

# sgcl::txt::patch_error::message

```cpp
string message() const noexcept;
```

Returns why, in a few words.

## Parameters

None.

## Return value

The reason.

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
    println("{}", txt::apply_patch("a\n", "@@ -1,2 +1 @@\n-a\n").error().message());
}
```

Output:

```text
a hunk shorter than its header says
```

## See also

- [hunk](hunk.md)
- [sgcl::txt::patch_error](README.md)
