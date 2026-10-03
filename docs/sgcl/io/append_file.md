[sgcl](../README.md) › [io](README.md)

# sgcl::io::append_file, async_append_file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<void, error> append_file(const string& path, const string& text,
                                              permissions p = permissions(0666));
    /*(2)*/ expected<void, error> append_file(const string& path, const slice<const byte>& data,
                                              permissions p = permissions(0666));
    /*(3)*/ template<class T>
            expected<void, error> append_file(const string& path, const T& text,
                                              permissions p = permissions(0666));
    /*(4)*/ async::task<expected<void, error>> async_append_file(const string& path,
                                                                 const string& text,
                                                                 permissions p = permissions(0666))
                noexcept;
    /*(5)*/ async::task<expected<void, error>> async_append_file(const string& path,
                                                                 const slice<const byte>& data,
                                                                 permissions p = permissions(0666))
                noexcept;
    /*(6)*/ template<class T>
            async::task<expected<void, error>> async_append_file(const string& path, const T& text,
                                                                 permissions p = permissions(0666));
}
```

The bytes added at the end of the file at `path` in one call: opened with `write | create | append`
([open_flags](open_flags.md)), created when it is not there, the bytes stored, closed, the close's error reported
too. A file it creates gets the permissions `p`, masked by the umask. What a log is written with, one line a call.

- (1–3) On the calling thread.
- (4–6) The same for a task, on the [blocking pool](../async/spawn_blocking.md): a disk has no readiness to wait for.

1. The characters of a [string](../core/string.md).
2. Bytes: a `vector<byte>`, an [array](../core/array.md), a [buffer](buffer.md)'s data, any slice of them.
3. A literal or a character array (to its first NUL), or a `std::string_view`: the overload takes part only for
   these, an exact match where the conversions to a `string` and to bytes would tie.
4. As (1); the task holds the string.
5. As (2). Bytes without an owner (a plain array, a `std::span`) are copied into a managed block before the write
   starts, since the pool's thread may outlive the frame of a task let go of; a slice with an owner (a `vector`, a
   [buffer](buffer.md)'s data) is written as it is.
6. As (3); the text is copied into a string at the call, which the task then holds.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |
| `text` | the characters to add |
| `data` | the bytes to add |
| `p` | the permissions of a file it creates, before the umask |

## Return value

Nothing, or the [error](error.md) of the step that failed: of [open](open.md) (`is_not_found()` for a directory on
the way that is not there, `is_permission()`, `EISDIR` for a directory), of the write (`ENOSPC` for a full disk) or
of the close.

## Complexity

Linear in the size of what is written.

## Exceptions

- (1–3) `std::system_error` when the path names a FIFO or a device, which the file writes on the
  [reactor](../async/readable.md), the write has to wait, and the thread of the reactor, which its first use starts,
  cannot be made.
- (4–5) None: the task's own exceptions are the task's.
- (6) `length_error` when the text is longer than a string holds (4 GiB).

## Notes

With `append`, the system puts every write at the end of the file at the moment it is made: processes appending to
one log at once do not write over each other's bytes.

## Example

```cpp
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    io::append_file("app.log", "started\n");  // created
    string line = "working\n";
    io::append_file("app.log", line);
    io::append_file("app.log", std::string_view("stopped\n"));
    print("{}", *io::read_text("app.log"));
}
```

Output:

```text
started
working
stopped
```

A task adds to a file without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> note(string line) {
    co_await io::async_append_file("task.log", line);
}

int main() {
    async::run(note("first\n"));
    async::run(note("second\n"));
    print("{}", *io::read_text("task.log"));
}
```

Output:

```text
first
second
```

## See also

- [write_file](write_file.md): the whole file written over
- [open_flags](open_flags.md): `append`
- [sgcl::io::file](file.md)
