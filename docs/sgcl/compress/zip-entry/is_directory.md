[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [entry](README.md)

# sgcl::compress::zip::entry::is_directory

```cpp
bool is_directory() const noexcept;
```

Checks whether the entry is a directory: whether its name ends in `/`, as the format marks one.

## Parameters

None.

## Return value

`true` when the name ends in `/`.

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
    (void)w.create("logs/");
    (void)w.add("logs/app.log", "one\n");
    (void)w.close();

    for (auto& e : compress::zip::archive::from(archive.data())->entries()) {
        println("{}: {}", e.name, e.is_directory());
    }
}
```

Output:

```text
logs/: true
logs/app.log: false
```

## See also

- [sgcl::compress::zip::entry](README.md)
