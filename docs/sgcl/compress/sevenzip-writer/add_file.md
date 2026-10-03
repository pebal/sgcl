[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::add_file

```cpp
/*(1)*/ expected<void, error> add_file(const string& path);
/*(2)*/ expected<void, error> add_file(const string& path, const string& name);
```

Writes the file at `path` as a whole entry, with the file's mode and modification time, as
[sevenzip::create](../sevenzip-create.md) writes each file of a tree: the file is read a block at a time, encoded as
it comes and never held whole. A symbolic link is followed: the entry holds the bytes of the file it names.

1. The entry is named by the file's own name, [io::path::base](../../io/path/base.md)`(path)`: `"logs/today.txt"`
   gives `today.txt`.
2. The entry is named `name`, `/` between the parts (`"logs/today.txt"`).

- (1–2) Unlike [add](add.md), it returns its error, since a file can fail where bytes in memory cannot. What is
  checked before anything is begun is the error of this call alone, and the archive goes on: a path that does not
  open, one that is not a regular file (a directory, a device), a name that ends in `/` (a directory's). A writer
  that failed or is closed gives its error. A read of the file that fails once the entry has begun would leave the
  entry short of the file: it is kept as the writer's error, as a failure of the output is.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to add |
| `name` | the entry's name, `/` between the parts |

## Return value

Nothing, or the [error](../error.md): `errc::io` with the file's error in `io_error()` when the path does not open
(`is_not_found()`), `errc::invalid_argument` for a path that is not a regular file or a name that ends in `/`; or the
writer's error, kept as its first ([last_error](last_error.md)): a name the archive cannot hold, a failure of the
output or of the file's read, an entry after the close, the error kept from before.

## Complexity

Linear in the size of the file.

## Exceptions

What the file's read throws (`std::system_error` for a file read through the reactor whose thread cannot be
started).

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
    compress::sevenzip::writer w(archive);
    (void)w.add_file("logs/today.txt");
    (void)w.add_file("notes.txt", "docs/notes.txt");
    println("{}", w.add_file("notes.txt", "notes/").error().message());
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
    for (auto& e : a->entries()) {
        println("{}: {} bytes", e.name, e.size);
    }
}
```

Output:

```text
7z: a file's entry named as a directory: notes/
today.txt: 16 bytes
docs/notes.txt: 5 bytes
```

## See also

- [add](add.md): an entry of bytes in memory
- [create](create.md): an entry written as a stream
- [sevenzip::create](../sevenzip-create.md): a whole directory
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
