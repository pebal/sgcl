[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [entry](README.md)

# sgcl::compress::sevenzip::entry::is_symlink

```cpp
bool is_symlink() const noexcept;
```

Checks whether the entry is a symbolic link: whether its attributes carry a POSIX mode (0x8000) whose type is a link.
Its data is then the link's target, as 7-Zip writes links on Unix.

## Parameters

None.

## Return value

`true` when the POSIX attributes name a symbolic link.

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
    w.add("notes.txt", "remember the milk\n");
    w.add("latest", "notes.txt", {.symlink = true});
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        println("{}: {}", e.name, e.is_symlink());
    }
    println("-> {}", string(slice<const byte>(*a->read("latest"))));
}
```

Output:

```text
notes.txt: false
latest: true
-> notes.txt
```

## See also

- [sgcl::compress::sevenzip::entry](README.md)
