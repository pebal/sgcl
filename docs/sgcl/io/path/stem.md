[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::stem

```cpp
string stem(const string& path) noexcept;
```

Returns the last element of `path` without its extension: [base](base.md) up to its last dot, what is left when
[ext](ext.md) is taken away. Go has no such function; `std::filesystem::path::stem` is the same but for a name that
begins with its only dot, which is all extension here as in Go's `Ext` (`.bashrc` has the stem `""`).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path |

## Return value

The last element up to its last dot; the whole last element when it has no dot.

## Complexity

Linear in the length of `path`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"/a/b/c.tar.gz", "notes.txt", ".bashrc", "Makefile"}) {
        println("\"{}\" -> \"{}\"", p, io::path::stem(p));
    }
}
```

Output:

```text
"/a/b/c.tar.gz" -> "c.tar"
"notes.txt" -> "notes"
".bashrc" -> ""
"Makefile" -> "Makefile"
```

Every `*.jpeg` under a directory renamed to `*.jpg`:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("photos/2024");
    for (const char* name : {"photos/a.jpeg", "photos/b.png", "photos/2024/c.jpeg"}) {
        (void)io::write_file(name, string("..."));
    }
    (void)io::walk_dir("photos", [](const io::directory_entry& e, const optional<io::error>&) {
        if (io::path::ext(e.path) == ".jpeg") {
            auto to = io::path::join(io::path::dir(e.path), io::path::stem(e.path) + ".jpg");
            if (auto renamed = io::rename(e.path, to)) {
                println("{} -> {}", e.path, io::path::base(to));
            } else {
                eprintln(renamed.error().message());
            }
        }
        return io::walk_action::next;
    });
}
```

Output:

```text
photos/2024/c.jpeg -> c.jpg
photos/a.jpeg -> a.jpg
```

## See also

- [ext](ext.md): the extension
- [base](base.md): the last element
- [walk_dir](../walk_dir.md), [rename](../rename.md): the walk and the rename of the second program
- [sgcl::io::path](../path.md)
