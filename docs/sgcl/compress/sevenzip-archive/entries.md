[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](../sevenzip-archive.md)

# sgcl::compress::sevenzip::archive::entries

```cpp
slice<const entry> entries() const noexcept;
```

Returns every [entry](../sevenzip-entry.md) of the archive, in the archive's order: what its header says of each.
The slice keeps the entries alive, shared with the archive and its copies. With the header encrypted, the entries
are there only when the archive was opened with the password.

## Parameters

None.

## Return value

The entries.

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
    w.add_directory("docs");
    w.add("docs/a.txt", string("a").repeat(1000));
    w.add("docs/b.txt", "b");
    (void)w.close();

    for (auto& e : compress::sevenzip::archive::from(archive.data())->entries()) {
        println("{:10} {:5} {}", e.name, e.size, e.is_directory ? "directory" : "file");
    }
}
```

Output:

```text
docs           0 directory
docs/a.txt  1000 file
docs/b.txt     1 file
```

## See also

- [find](find.md), [walk](walk.md)
- [sgcl::compress::sevenzip::archive](../sevenzip-archive.md)
