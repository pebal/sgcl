[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the archive was closed: whether [close](close.md) ran, whatever it gave. After it an entry or a write
to one is refused.

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
    compress::sevenzip::writer w(archive);
    w.add("a.txt", "a");
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
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
