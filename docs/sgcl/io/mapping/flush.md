[sgcl](../../README.md) › [io](../README.md) › [mapping](../mapping.md)

# sgcl::io::mapping::flush

```cpp
expected<void, error> flush() const noexcept;
```

Gives the writes of a writable shared mapping to the file and waits until they are on the disk: `msync` with
`MS_SYNC`; `FlushViewOfFile` and `FlushFileBuffers` on Windows. For any other mapping (read only, private, empty)
there is nothing to do, and it succeeds. Without it the writes reach the file anyway, when the system writes the
pages back, and a read of the file sees them at once (one page cache): `flush` is for a file that must be on the
disk, as [file::sync](../file/sync.md) is.

## Parameters

None.

## Return value

Nothing, or an [error](../error.md): `is_closed()` once the mapping is [closed](close.md) (operation `flush`), the
error of `msync` otherwise (operation `msync`).

## Complexity

Linear in the number of pages written since the last flush: the time of the disk.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("counter.txt", string("0"));
    io::mapping m = io::map("counter.txt", {.writable = true});
    m.writable_data()[0] = byte('7');
    println("{}", bool(m.flush()));
    println("{}", io::read_text("counter.txt").value());
    (void)m.close();
    println("{}", m.flush().error().message());
}
```

Output:

```text
true
7
flush counter.txt: stream closed
```

## See also

- [writable_data](writable_data.md): the bytes to write into
- [close](close.md): the file given back
- [sgcl::io::mapping](../mapping.md)
