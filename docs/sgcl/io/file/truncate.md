[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::truncate, async_truncate

```cpp
/*(1)*/ expected<void, error> truncate(uint64_t size) const noexcept;
/*(2)*/ async::task<expected<void, error>> async_truncate(uint64_t size) const noexcept;
```

Makes the file `size` bytes long: the `ftruncate(2)` of the descriptor. A file longer than `size` loses its bytes
past it; a shorter one grows by zeros. The position does not move, and may be left past the end.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `size` | the new size of the file, in bytes |

## Return value

Nothing, or the [error](../error.md), its operation `truncate` and its path the file's: `errc::closed` for a closed
file, otherwise the `errno` of `ftruncate(2)` (`EINVAL`, or `EBADF` on some systems, for a file not opened for
writing; `EINVAL` for a pipe).

## Complexity

One system call.

## Exceptions

None.

## Notes

A file is sized so before a writable [map](../map.md) of it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto flags = io::open_flags::read | io::open_flags::write | io::open_flags::create;
    io::file f = io::open("sized.bin", flags);
    f.write("0123456789");
    f.truncate(4);
    println("{} {}", f.stat()->size, *f.tell());
    f.truncate(6);
    vector<byte> all(6);
    f.read_at(all, 0);
    println("{}", all);
    println("{}", static_cast<bool>(io::open("sized.bin").value().truncate(0)));
}
```

Output:

```text
4 10
[48, 49, 50, 51, 0, 0]
false
```

## See also

- [stat](stat.md): the size of the file
- [map](../map.md): a file mapped into memory
- [sgcl::io::file](../file.md)
