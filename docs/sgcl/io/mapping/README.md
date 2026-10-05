[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::mapping

```cpp
#include "sgcl/io/mapping.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class mapping final;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`io::mapping` is a file mapped into memory: its bytes as a [slice](../../core/slice/README.md), read and written where they
lie, the operating system bringing the pages in as they are touched (`mmap`; `MapViewOfFile` on Windows). It is made
by [map](../map.md): the whole file for reading by default, a writable mapping, a private one or a range of the file
by [map_options](../map_options.md). `io::mapping m = io::map(p);` takes the mapping out of the `expected`, and throws
its error when there is none ([expected](../../core/expected/README.md)).

A `mapping` is a handle of one word, a `tracked_ptr` to the region inside, as a [file](../file/README.md) is: a copy is the
same mapping. [data](data.md) is the mapped bytes as a `slice<const byte>` whose owner is the region, so a
slice kept after the last handle is gone still reads the mapping: the region is unmapped when nothing holds it any
more, a handle or a slice, by the destructor on the collector's thread, or given back at once by
[close](close.md). A writable mapping gives [writable_data](writable_data.md), the same bytes as a
`slice<byte>`. A [shared_memory](../shared_memory/README.md) is the same kind of region under another handle, with only
`data()`, a `slice<byte>`, since its region is always read and written.

Go has no mapping in its standard library beyond `syscall.Mmap`, a `[]byte` given back by `syscall.Munmap` by hand,
and `golang.org/x/exp/mmap`, a reader of a mapped file; here the bytes are a slice that keeps the mapping alive, and
[shared_memory](../shared_memory/README.md) is the named region between processes. `std` has no mapping at all.

## Rules

- The region is outside the managed heap: put only trivial data in it — never a tracked_ptr or a library handle.
- A range past the end of the file is an error (`std::errc::invalid_argument`), for a writable mapping as for a
  read-only one: the file is never extended by a mapping, and a byte past its end would be a `SIGBUS`. To write a
  file of a given size through a mapping, make it that size first ([file::truncate](../file/truncate.md)), then map
  it.
- The size is fixed when the file is mapped. A file that grows afterwards is not seen past the old end
  ([size](size.md) stays; a new `map` sees the rest); a file cut shorter under a mapping is a `SIGBUS` on a
  page past the new end, as in every language that maps files: a mapping of a file other programs may truncate is
  read with that in mind.
- An empty file, or an empty range (`offset` at the end), maps to an empty mapping: `size()` 0, `data()` empty, no
  system mapping made.
- The mapping holds a descriptor of its own: `map(path)` the one it opened, `map(f)` a duplicate, so the
  [file](../file/README.md) may be closed after `map(f)`.
- [close](close.md) gives the file back now and leaves zeros under a slice taken before; the destructor
  unmaps a mapping nobody closed. On Windows the handles are closed and the view stays mapped until the region is
  collected.
- Any thread or task may read and write the region at once; the bytes are plain memory, with no ordering between
  threads beyond what the program makes (atomics in the region are the program's own business).
- A `mapping` is a [req::handle](../../core/req/handle.md), so an [atomic](../../core/atomic-handle/README.md) of it
  compares and swaps by identity.
- A `mapping` made by its default constructor holds none (`!m`); an operation on it is a contract violation,
  asserted in a debug build.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mapping.md) | an empty handle |

#### Element access

| Function | Description |
|---|---|
| [data](data.md) | the mapped bytes, read only |
| [writable_data](writable_data.md) | the mapped bytes to write into |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the length of the range |

#### File operations

| Function | Description |
|---|---|
| [flush](flush.md) | the writes given to the file and waited for |
| [close](close.md) | gives the file back now |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | checks whether the mapping was closed |
| [operator bool](operator_bool.md) | checks whether the handle holds a mapping |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same mapping |

## Example

A range of the file from any byte; the slice outlives the handle.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("record.txt", string("header:payload:trailer"));
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

- [map](../map.md), [map_options](../map_options.md): how a file is mapped
- [shared_memory](../shared_memory/README.md): a named region between processes, the same region under another handle
- [file::truncate](../file/truncate.md): a file sized before a writable mapping; [slice](../../core/slice/README.md): what
  `data()` is
- `tests/io/mapping.cpp`: read, write, `flush` and `close` seen in the file, a private mapping, an offset inside a
  page, an empty file and a file that grows, a mapping of an open `file`, `close` leaving zeros under an old slice, a
  slice kept after the handle, the errors (a missing file, a range past the end read only and writable, a writable
  mapping of a read-only file), `rooted` handles and slices, copies and an atomic of the handle
