[sgcl](../../README.md) › [txt](../README.md) › [patch_error](README.md)

# sgcl::txt::patch_error::hunk

```cpp
size_t hunk() const noexcept;
```

Returns the hunk that failed, counted from 1 in the patch; 0 when the patch has no hunk at all.

## Parameters

None.

## Return value

The hunk.

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
    string patch = "@@ -1 +1 @@\n-a\n+A\n@@ -3 +3 @@\n-c\n+C\n";
    println("{}", txt::apply_patch("a\nb\n", patch).error().hunk());
    println("{}", txt::apply_patch("a\n", "not a patch\n").error().hunk());
}
```

Output:

```text
2
0
```

## See also

- [line](line.md)
- [sgcl::txt::patch_error](README.md)
