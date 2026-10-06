[sgcl](../../README.md) › [txt](../README.md) › [tab_writer](README.md)

# sgcl::txt::tab_writer::flush

```cpp
string flush();
```

Gives back everything still held, aligned, as if the text had ended: the cell being read ends the last line, which
keeps having no line break when it had none, and the block of columns ends. The writer is empty after it and takes
text again.

## Parameters

None.

## Return value

The lines held, aligned; empty when nothing was.

## Complexity

Linear in the length of the text held.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::tab_writer w;
    w.write("a\tb\nccc\td");
    println("[{}]", w.flush());
    println("[{}]", w.flush());
}
```

Output:

```text
[a   b
ccc d]
[]
```

## See also

- [write](write.md)
- [sgcl::txt::tab_writer](README.md)
