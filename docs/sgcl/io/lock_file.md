[sgcl](../README.md) › [io](README.md)

# sgcl::io::lock_file, async_lock_file

```cpp
#include "sgcl/io/lock.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<file_lock, error> lock_file(const file& f, const lock_options& options = {}) noexcept;    // (1)
    expected<file_lock, error> lock_file(const string& path,                                           // (2)
                                         const lock_options& options = {}) noexcept;
    async::task<expected<file_lock, error>> async_lock_file(const file& f,                             // (3)
                                                            lock_options options = {}) noexcept;
    async::task<expected<file_lock, error>> async_lock_file(const string& path,                        // (4)
                                                            lock_options options = {}) noexcept;
}
```

Locks a file between processes, advisory as every lock on POSIX: the whole file by `flock`, the lock `flock(1)` and
the Maildir programs take, or a byte range by an open-file-description lock (`fcntl` `F_OFD_SETLK`, Linux 3.15 and
later, macOS) when [lock_options](lock_options.md) give one; shared or exclusive. Both belong to the open file, not to
the process: two opens of one file in a process exclude each other as two processes do, and the close of another
descriptor of the file drops nothing, which the classic `fcntl` locks of POSIX (the process's) would. The returned
[file_lock](file_lock/README.md) gives the lock back by its [unlock](file_lock/unlock.md) or at the end of its scope.

1. The file locked, waiting as long as it takes, trying once for a zero `timeout`, waiting at most `timeout`
   otherwise (macOS's kernel waits for a range itself; else tries with sleeps of 1 ms doubling to 50 ms).
2. The one-line form: the lock file at `path` opened for reading and writing, created when missing (0666 less the
   umask), and locked as (1); the guard holds it open.
3. (1) for a task: the lock tried, and a sleep on the timers between the tries (1 ms doubling to 50 ms), so that no
   thread is held for the length of somebody else's lock.
4. (2) for a task, the file opened on the [blocking pool](../async/spawn_blocking.md).

Go has no file lock in its standard library (`syscall.Flock` by hand); Rust's `File::lock` and `try_lock` are the
nearest. `flock` and `fcntl` locks are separate families, which one system lets see each other and another does not:
a file is locked one way.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the file, open |
| `path` | the lock file's path |
| `options` | shared or exclusive, the range, the timeout ([lock_options](lock_options.md)) |

## Return value

The guard ([file_lock](file_lock/README.md)), or the [error](error/README.md), its operation `lock` and its path the
file's:

- `EWOULDBLOCK` for a zero timeout that found the lock held, `ETIMEDOUT` for a timeout that passed: both
  `is_timeout()`;
- `errc::closed` for a closed file;
- `EINVAL` for a range past `INT64_MAX`, `ENOTSUP` for a range where the system has no open-file-description lock;
- (2, 4) the errors of [open](open.md) (`is_not_found()` for a missing directory);
- the `errno` of `flock` or `fcntl` otherwise.

## Complexity

One system call when the lock is free; the wait is the holder's.

## Exceptions

- (1–2) None.
- (3–4) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("app.lock").value();  // one instance of the program at a time
    auto second = io::lock_file("app.lock", {.timeout = duration::zero()});
    println("{} {}", second.error().is_timeout(), second.error().message());

    io::file data = io::open("data.bin", io::open_flags::read | io::open_flags::write | io::open_flags::create).value();
    io::file again = io::open("data.bin", io::open_flags::read | io::open_flags::write).value();
    io::file_lock record = io::lock_file(data, {.offset = 0, .length = 512}).value();
    auto other = io::lock_file(again, {.offset = 512, .length = 512, .timeout = duration::zero()});
    auto overlap = io::lock_file(again, {.offset = 256, .length = 512, .timeout = duration::zero()});
    println("{} {}", other.has_value(), overlap.has_value());
}
```

Output:

```text
true lock app.lock: Resource temporarily unavailable
true false
```

## See also

- [file_lock](file_lock/README.md): the guard
- [lock_options](lock_options.md), [lock_mode](lock_mode.md)
- [file](file/README.md), [open](open.md)
