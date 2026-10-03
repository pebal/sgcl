[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::sync, async_sync

```cpp
expected<void, error> sync() const noexcept;                       // (1)
async::task<expected<void, error>> async_sync() const noexcept;    // (2)
```

Waits until what was written to the file reaches the disk: the `fsync(2)` of the descriptor. A write returns once
the kernel has the data; a sync returns once the disk has it, so that a crash of the machine after it loses none.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md): an fsync waits for the disk.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md), its operation `sync` and its path the file's: `errc::closed` for a closed file,
otherwise the `errno` of `fsync(2)` (`EIO`, `EINVAL` for a descriptor that cannot be synced, such as a pipe).

## Complexity

One system call, which waits for the disk to store what the file has in the kernel's memory.

## Exceptions

None.

## Notes

A file opened with `open_flags::sync` ([open_flags](../open_flags.md)) makes every write reach the disk before it
returns, a sync after each.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<expected<void, io::error>> save(io::file f, string state) {
    auto written = co_await f.async_write(state);
    if (!written) co_return unexpected(written.error());
    co_return co_await f.async_sync();
}

int main() {
    io::file f = io::create("state.txt");
    println("{}", static_cast<bool>(f.sync()));
    println("{}", static_cast<bool>(async::run(save(f, "saved"))));
    println("{}", *io::read_text("state.txt"));
}
```

Output:

```text
true
true
saved
```

## See also

- [write, async_write](write.md): what a sync stores
- [open_flags](../open_flags.md): `sync`, a sync after every write
- [sgcl::io::file](README.md)
