[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [entry](../zip-entry.md)

# sgcl::compress::zip::entry::is_symlink

```cpp
bool is_symlink() const noexcept;
```

Checks whether the entry is a symbolic link, as its Unix attributes say: its data is then the link's target.
[create](../zip-create.md) archives a symbolic link as one, and [extract](../zip-extract.md) makes it last, its
target kept inside the directory.

## Parameters

None.

## Return value

`symlink`.

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
    (void)(*w.create({.name = "latest", .symlink = true})).write("v2/readme.txt");
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        println("{} {} -> {}", e.name, e.is_symlink(), string(slice<const byte>(*a->read(e))));
    }
}
```

Output:

```text
latest true -> v2/readme.txt
```

## See also

- [sgcl::compress::zip::entry](../zip-entry.md)
