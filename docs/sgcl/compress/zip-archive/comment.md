[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::comment

```cpp
const string& comment() const noexcept;
```

Returns the archive's comment, the text the end record carries after the central directory (at most 65 535 bytes);
the writer's [set_comment](../zip-writer/set_comment.md) writes it.

## Parameters

None.

## Return value

The comment; empty when the archive has none.

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
    (void)w.set_comment("built on monday");
    (void)w.close();
    println("{}", compress::zip::archive::from(archive.data())->comment());
}
```

Output:

```text
built on monday
```

## See also

- [set_comment](../zip-writer/set_comment.md)
- [sgcl::compress::zip::archive](../zip-archive.md)
