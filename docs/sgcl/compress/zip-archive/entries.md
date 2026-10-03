[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::entries

```cpp
slice<const entry> entries() const noexcept;
```

Returns every [entry](../zip-entry.md) of the archive, in the order of its central directory: what each record
says, the name, the time, the sizes, the CRC-32, the method, the mode. The slice keeps the entries alive, shared
with the archive and its copies.

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
    compress::zip::writer w(archive);
    (void)w.create("docs/");
    (void)w.add("docs/a.txt", string("a").repeat(1000));
    (void)w.add("docs/b.txt", "b");
    (void)w.close();

    for (auto& e : compress::zip::archive::from(archive.data())->entries()) {
        println("{:12} {:5} {:5}", e.name, e.size, e.compressed_size);
    }
}
```

Output:

```text
docs/            0     0
docs/a.txt    1000    10
docs/b.txt       1     3
```

## See also

- [find](find.md): an entry by its name
- [sgcl::compress::zip::archive](../zip-archive.md)
