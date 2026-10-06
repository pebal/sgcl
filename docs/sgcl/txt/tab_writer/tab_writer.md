[sgcl](../../README.md) › [txt](../README.md) › [tab_writer](README.md)

# sgcl::txt::tab_writer::tab_writer

```cpp
explicit tab_writer(const tab_options& o = {});
```

Constructs a writer holding nothing, which sets its columns by `o`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the [tab_options](../tab_options.md) |

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
    txt::tab_writer w({.pad_char = '.'});
    print("{}", w.write("a\tb\nccc\td\n"));
    print("{}", w.flush());
}
```

Output:

```text
a...b
ccc.d
```

## See also

- [write](write.md)
- [align_tabs](../align_tabs.md)
- [sgcl::txt::tab_writer](README.md)
