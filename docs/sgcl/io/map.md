[sgcl](../README.md) › [io](README.md)

# sgcl::io::map

```cpp
#include "sgcl/io/mapping.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<mapping, error> map(const string& path, const map_options& options) noexcept;    // (1)
    expected<mapping, error> map(const string& path) noexcept;                                // (2)
    expected<mapping, error> map(const file& f, const map_options& options = {}) noexcept;    // (3)
}
```

Maps a file into memory: its bytes as a [slice](../core/slice/README.md), read and written where they lie, the operating
system bringing the pages in as they are touched (`mmap`; `MapViewOfFile` on Windows). The result is a
[mapping](mapping/README.md), a handle; `io::mapping m = io::map(p);` takes it out of the `expected`, and throws its error
when there is none ([expected](../core/expected/README.md)).

1. The file at `path`, as the [options](map_options.md) say: a writable mapping, a private one, or a range. The file
   is opened for reading, and for writing as well for a writable shared mapping.
2. The whole file at `path`, read only.
3. An open file. The mapping holds a descriptor of its own (a `dup`), so the file may be closed after; a writable
   shared mapping needs a file opened for reading and writing.

The mapping holds its descriptor until it is closed or collected: the one (1) and (2) opened, the duplicate of (3).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |
| `f` | the open file |
| `options` | read only or writable, shared or private, the range |

## Return value

The mapping, or an [error](error/README.md) whose `path()` is the file's:

- `is_not_found()` for a missing file, and the other errors of the open (operation `open`);
- `is_permission()` for a writable shared mapping of a file the program may not write: (1) when the open is
  refused, (3) when the file was opened for reading alone (operation `mmap`, `EACCES`);
- `std::errc::invalid_argument` for a range past the end of the file (operation `map`);
- `std::errc::file_too_large` for a range longer than a `size_t` holds, in a 32-bit program (operation `map`);
- `is_closed()` for (3) of a closed file (operation `map`);
- the error of `mmap` otherwise (operation `mmap`).

## Complexity

Constant: the pages are brought in when they are touched, not by the map.

## Exceptions

None.

## Example

A file read through a mapping: the bytes where they lie, no read calls.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("lines.txt", string("one\ntwo\nthree\n"));
    io::mapping m = io::map("lines.txt");
    size_t lines = 0;
    for (byte b : m.data()) {
        lines += b == byte('\n');
    }
    println("{} bytes, {} lines", m.size(), lines);
}
```

Output:

```text
14 bytes, 3 lines
```

The errors, and an open file mapped and closed:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::map("missing.txt").error().message());
    (void)io::write_file("short.txt", string("0123456789"));
    println("{}", io::map("short.txt", {.offset = 4, .length = 7}).error().message());

    io::file f = io::open("short.txt");
    io::mapping m = io::map(f, {.offset = 4});
    (void)f.close();
    println("{}", string(m.data()));
    println("{}", io::map(f).error().message());
}
```

Output:

```text
open missing.txt: No such file or directory
map short.txt: Invalid argument
456789
map short.txt: stream closed
```

## See also

- [mapping](mapping/README.md): what it returns
- [map_options](map_options.md): writable, private, a range
- [shared_memory](shared_memory/README.md): a named region between processes
