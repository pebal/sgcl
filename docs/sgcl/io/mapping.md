# sgcl::io::mapping — map, map_options

```cpp
#include "sgcl/io/mapping.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    struct map_options {
        bool writable = false;   // read only by default
        bool shared = true;      // a writable mapping writes to the file; false: a private copy on write
        uint64_t offset = 0;     // any byte
        uint64_t length = 0;     // 0: to the end of the file
    };
    class mapping final;         // a handle of one word

    expected<mapping, error> map(const string& path);                              // the whole file, read only
    expected<mapping, error> map(const string& path, const map_options& options);
    expected<mapping, error> map(const file& f, const map_options& options = {});
}
```

A file mapped into memory: its bytes as a [`slice`](../core/slice.md), read and written where they lie, the operating system bringing the pages in as they are touched (`mmap`; `MapViewOfFile` on Windows). `io::map(path)` maps the whole file for reading; `map_options` asks for a writable mapping, a private one, or a range of the file.

A `mapping` is a handle of one word, a `tracked_ptr` to the region inside, as a [`file`](file.md) is: a copy is the same mapping. `data()` is the mapped bytes as a `slice<const byte>` whose owner is the region, so a slice kept after the last handle is gone still reads the mapping: the region is unmapped when nothing holds it any more, a handle or a slice, by the destructor on the collector's thread. A writable mapping gives `writable_data()`, the same bytes as a `slice<byte>` (a `shared_memory` has only `data()`, a `slice<byte>`, since its region is always read and written). `io::mapping m = io::map(p);` takes the mapping out of the `expected`, and throws its error when there is none ([expected](../core/expected.md)).

The members of `map_options`:

- `writable` (default `false`): the mapping may be written through `writable_data()`. A read-only mapping is opened and mapped for reading alone.
- `shared` (default `true`): with `writable`, writes go to the file itself (`MAP_SHARED`), as Go's mmap packages and Python's `ACCESS_WRITE` do; other processes mapping or reading the file see them, and `flush()` waits until they are on the disk. `false` is a private copy on write (`MAP_PRIVATE`): the program's writes stay its own and the file is untouched, which also needs no permission to write the file. A read-only mapping ignores it.
- `offset` (default `0`): where the range starts in the file, any byte: the mapping is aligned down to a page (an allocation granule of 64 KB on Windows) inside, and `data()` starts at the byte asked for.
- `length` (default `0`): how many bytes from `offset`; `0` is the rest of the file.

## Rules

- The region is outside the managed heap: put only trivial data in it — never a tracked_ptr or a library handle.
- A range past the end of the file is an error (`std::errc::invalid_argument`), for a writable mapping as for a read-only one: the file is never extended by a mapping, and a byte past its end would be a `SIGBUS`. To write a file of a given size through a mapping, make it that size first (`file::truncate`), then map it.
- The size is fixed when the file is mapped. A file that grows afterwards is not seen past the old end (`size()` stays; a new `map` sees the rest); a file cut shorter under a mapping is a `SIGBUS` on a page past the new end, as in every language that maps files: a mapping of a file other programs may truncate is read with that in mind.
- An empty file, or an empty range (`offset` at the end), maps to an empty mapping: `size()` 0, `data()` empty, no system mapping made.
- `close()` gives the file back now: the descriptor is closed and the range is replaced by zero pages (`MAP_FIXED`, one call), so a slice taken before the close reads zeros rather than faulting; `data()` is empty after it and `size()` 0, `flush()` answers `errc::closed`. The address range itself goes back to the system with the region. On Windows the handles are closed and the view stays mapped until the region is collected.
- The mapping holds a descriptor of its own: `map(path)` the one it opened, `map(f)` a duplicate, so the `file` may be closed after `map(f)`. A writable shared mapping needs a file open for writing: `map(path, {.writable = true})` opens it so, and its error answers `is_permission()` when the program may not write it; `map(f, {.writable = true})` of a file opened for reading alone fails the same way.
- Any thread or task may read and write the region at once; the bytes are plain memory, with no ordering between threads beyond what the program makes (atomics in the region are the program's own business).

## Members

### map_options

```cpp
struct map_options {
    bool writable = false;
    bool shared = true;
    uint64_t offset = 0;
    uint64_t length = 0;
};
```

Each member is described above; a designated initializer names what differs from the defaults: `io::map(p, {.writable = true, .offset = 4096})`.

### mapping

```cpp
mapping() noexcept;                                       // no mapping: !m
slice<const byte> data() const noexcept;                   // the mapped bytes; empty once closed
slice<byte> writable_data() const noexcept;                // the same to write: a writable mapping alone (a contract violation on one read only)
size_t size() const noexcept;                              // the length of the range, 0 once closed
expected<void, error> flush() const;                       // msync(MS_SYNC); FlushViewOfFile and FlushFileBuffers on Windows
expected<void, error> close() const;  bool is_closed() const noexcept;
explicit operator bool() const noexcept;                   // whether the handle holds a mapping
friend bool operator==(const mapping& a, const mapping& b) noexcept;   // the same region
```

`flush()` waits until the writes of a writable shared mapping are in the file; for any other mapping it has nothing to do and succeeds. Without it the writes reach the file anyway, when the system writes the pages back, and a read of the file sees them at once (one page cache). `writable_data()` of a read-only mapping stops a debug build and returns an empty slice otherwise.

### map

```cpp
expected<mapping, error> map(const string& path);
expected<mapping, error> map(const string& path, const map_options& options);
expected<mapping, error> map(const file& f, const map_options& options = {});
```

The error answers `is_not_found()` for a missing file, `is_permission()` for a writable shared mapping of a file the program may not write, `std::errc::invalid_argument` for a range past the end; its `op()` is `open`, `map` or `mmap` and its `path()` the file's.

## Example

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

// A file read through a mapping: the bytes where they lie, no read calls
int main() {
    io::write_file("lines.txt", string("one\ntwo\nthree\n"));
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

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

// Written in place: the file has the bytes after flush
int main() {
    io::write_file("greeting.txt", string("hello world"));
    io::mapping m = io::map("greeting.txt", {.writable = true});
    slice<byte> bytes = m.writable_data();
    bytes[0] = byte('H');
    bytes[6] = byte('W');
    m.flush();
    println("{}", io::read_text("greeting.txt").value());
}
```

Output:

```text
Hello World
```

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

// A range of the file from any byte; the slice outlives the handle
int main() {
    io::write_file("record.txt", string("header:payload:trailer"));
    slice<const byte> payload;
    {
        io::mapping m = io::map("record.txt", {.offset = 7, .length = 7});
        payload = m.data();
    }
    println("{}", string(payload));
}
```

Output:

```text
payload
```

## See also

- [shared_memory](shared_memory.md): a named region between processes, the same region under another handle
- [file](file.md): `truncate` to size a file before a writable mapping; [slice](../core/slice.md): what `data()` is
- `tests/io/mapping.cpp`: read, write, `flush` and `close` seen in the file, a private mapping, an offset inside a page, an empty file and a file that grows, a mapping of an open `file`, `close` leaving zeros under an old slice, a slice kept after the handle, the errors (a missing file, a range past the end read only and writable, a writable mapping of a read-only file), `rooted` handles and slices, copies and an atomic of the handle.
