[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::tab_writer

```cpp
#include "sgcl/txt/tab_writer.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class tab_writer;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::tab_writer` is [align_tabs](../align_tabs.md) streamed, Go's `tabwriter.Writer`: text is written to it
in pieces, held until its block of columns ends, and given back aligned. The module writes to no file, so the writer
gives its lines back rather than writing them; a program prints them or appends them where it wants. A cell is as
wide as a terminal shows it, where Go counts code points.

## Rules

A handle of one word: a copy is the same writer, whose state it shares. A writer is used by one thread at a time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](tab_writer.md) | constructs a writer with its options |

#### Modifiers

| Function | Description |
|---|---|
| [write](write.md) | takes text, gives back the lines whose columns it ended |
| [flush](flush.md) | gives back everything still held |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::tab_writer w;
    for (int i : range(3)) {
        print("{}", w.write(txt::format("file_{}.txt\t{}\n", i, i * 1000 + 7)));
    }
    print("{}", w.flush());
}
```

Output:

```text
file_0.txt 7
file_1.txt 1007
file_2.txt 2007
```

## See also

- [align_tabs](../align_tabs.md)
- [tab_options](../tab_options.md)
- [sgcl::txt](../README.md)
