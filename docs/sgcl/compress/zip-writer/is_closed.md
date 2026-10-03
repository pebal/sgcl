[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the archive was closed: whether [close](close.md) ran, whatever it gave. After it a create, an add or a
write to an entry is refused.

## Parameters

None.

## Return value

`true` after a close, `false` before it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    (void)w.add("a.txt", "a");
    println("{}", w.is_closed());
    (void)w.close();
    println("{}", w.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md)
- [sgcl::compress::zip::writer](../zip-writer.md)
