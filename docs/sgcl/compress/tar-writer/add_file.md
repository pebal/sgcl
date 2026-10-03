[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](../tar-writer.md)

# sgcl::compress::tar::writer::add_file

```cpp
/*(1)*/ expected<void, error> add_file(const string& path);
/*(2)*/ expected<void, error> add_file(const string& path, const string& name);
```

Writes the file at `path` as a whole entry, its [header](write_header.md) and its data, with the file's mode,
modification time and size, as [tar::create](../tar-create.md) writes each file of a tree: the file is read a block
at a time and never held whole. A symbolic link is followed: the entry is a regular file with the bytes of the file
it names.

1. The entry is named by the file's own name, [io::path::base](../../io/path/base.md)`(path)`: `"logs/today.txt"`
   gives `today.txt`.
2. The entry is named `name`, `/` between the parts (`"logs/today.txt"`).

- (1–2) What is checked before anything is written is the error of this call alone, and the archive goes on: a path
  that does not open, one that is not a regular file (a directory, a device), a name that ends in `/` (a
  directory's). A writer that failed, is closed or is inside an entry's data gives its error. Once the header is
  written, a read of the file that fails, or a file that changed size while it was read, leaves the entry short of
  the size its header says: kept as the writer's error, as a failure of `out` is.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to add |
| `name` | the entry's name, `/` between the parts |

## Return value

Nothing, or the [error](../error.md): `errc::io` with the file's error in `io_error()` when the path does not open
(`is_not_found()`), `errc::invalid_argument` for a path that is not a regular file or a name that ends in `/`; or the
writer's error, kept as its first: what [write_header](write_header.md) refuses, a failure of `out` or of the file's
read, a file that changed size, the error kept from before.

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
    compress::tar::writer w(archive);
    (void)w.add_file("logs/today.txt");
    (void)w.add_file("notes.txt", "docs/notes.txt");
    println("{}", w.add_file("logs").error().message());
    (void)w.close();

    compress::tar::reader r(archive);
    while (auto e = r.next()) {
        if (!*e) {
            break;  // the end
        }
        println("{} {}: {}", (*e)->name, (*e)->size, r.read_all()->size());
    }
}
```

Output:

```text
tar: not a regular file: logs
today.txt 16: 16
docs/notes.txt 5: 5
```

## See also

- [write_header](write_header.md), [write](write.md): an entry written by parts
- [tar::create](../tar-create.md): a whole directory
- [sgcl::compress::tar::writer](../tar-writer.md)
