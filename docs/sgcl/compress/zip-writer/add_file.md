[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::add_file

```cpp
/*(1)*/ expected<void, error> add_file(const string& path);
/*(2)*/ expected<void, error> add_file(const string& path, const string& name);
```

Writes the file at `path` as a whole entry, deflated, with the file's mode and modification time, as
[zip::create](../zip-create.md) writes each file of a tree: the file is read a block at a time and never held whole.
A symbolic link is followed: the entry holds the bytes of the file it names.

1. The entry is named by the file's own name, [io::path::base](../../io/path/base.md)`(path)`: `"logs/today.txt"`
   gives `today.txt`.
2. The entry is named `name`, `/` between the parts (`"logs/today.txt"`).

- (1–2) What is checked before anything is written is the error of this call alone, and the archive goes on: a path
  that does not open, one that is not a regular file (a directory, a device), a name that ends in `/` (a
  directory's). A writer that failed or is closed gives its error. A read of the file that fails once the entry has
  begun would leave the entry short of the file: it is kept as the writer's error, as a failure of `out` is.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to add |
| `name` | the entry's name, `/` between the parts |

## Return value

Nothing, or the [error](../error.md): `errc::io` with the file's error in `io_error()` when the path does not open
(`is_not_found()`), `errc::invalid_argument` for a path that is not a regular file or a name that ends in `/`; or the
writer's error, kept as its first: what [create](create.md) refuses, a failure of `out` or of the file's read, the
error kept from before.

## Complexity

Linear in the size of the file.

## Exceptions

What the `write` of `out` throws; what the file's read throws (`std::system_error` for a file read through the
reactor whose thread cannot be started).

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("logs");
    (void)io::write_file("logs/today.txt", "started\nstopped\n");
    (void)io::write_file("notes.txt", "keep\n");

    io::buffer archive;
    compress::zip::writer w(archive);
    (void)w.add_file("logs/today.txt");
    (void)w.add_file("notes.txt", "docs/notes.txt");
    auto missing = w.add_file("missing.txt");
    println("{}", missing.error().message());
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        print("{}: {}", e.name, string(slice<const byte>(*a->read(e))));
    }
}
```

Output:

```text
input/output error: stat missing.txt: No such file or directory
today.txt: started
stopped
docs/notes.txt: keep
```

## See also

- [add](add.md): an entry of bytes in memory
- [create](create.md): an entry written as a stream
- [zip::create](../zip-create.md): a whole directory
- [sgcl::compress::zip::writer](../zip-writer.md)
