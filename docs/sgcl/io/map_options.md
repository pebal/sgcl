[sgcl](../README.md) › [io](README.md)

# sgcl::io::map_options

```cpp
#include "sgcl/io/mapping.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct map_options {
        bool writable = false;
        bool shared = true;
        uint64_t offset = 0;
        uint64_t length = 0;
    };
}
```

`io::map_options` is how [map](map.md) maps a file: read only by default, and the whole file. `writable` with
`shared`, the default, writes to the file itself, as Go's mmap packages and Python's `ACCESS_WRITE` do; `writable`
without `shared` is a private copy on write, the file untouched. A designated initializer names what differs from
the defaults: `io::map(p, {.writable = true, .offset = 4096})`.

## Member objects

| Member | Description |
|---|---|
| `writable` | the mapping may be written through [writable_data](mapping/writable_data.md); `false` by default, and a read-only mapping is opened and mapped for reading alone |
| `shared` | with `writable`, the writes go to the file itself (`MAP_SHARED`): other processes mapping or reading the file see them, and [flush](mapping/flush.md) waits until they are on the disk. `false` is a private copy on write (`MAP_PRIVATE`): the program's writes stay its own and the file is untouched, which also needs no permission to write the file. A read-only mapping ignores it. `true` by default |
| `offset` | where the range starts in the file, any byte: the mapping is aligned down to a page (an allocation granule of 64 KB on Windows) inside, and `data()` starts at the byte asked for; `0` by default |
| `length` | how many bytes from `offset`; `0`, the default, is the rest of the file. A range past the end of the file is an error, the file never extended |

## Example

A private mapping: the program's writes are its own, the file keeps its bytes.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("original.txt", string("original"));
    io::mapping m = io::map("original.txt", {.writable = true, .shared = false});
    m.writable_data()[0] = byte('O');
    println("{}", string(m.data()));
    println("{}", io::read_text("original.txt").value());
}
```

Output:

```text
Original
original
```

## See also

- [map](map.md): the function that takes the options
- [mapping](mapping.md): what it makes
- [file::truncate](file/truncate.md): a file sized before a writable mapping
