[sgcl](../README.md) › [io](README.md)

# sgcl::io::write_file, async_write_file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> write_file(const string& path, const string& text,                   // (1)
                                     permissions p = permissions(0666));
    expected<void, error> write_file(const string& path, const slice<const byte>& data,        // (2)
                                     permissions p = permissions(0666));
    template<class T>
    expected<void, error> write_file(const string& path, const T& text,                        // (3)
                                     permissions p = permissions(0666));
    async::task<expected<void, error>> async_write_file(const string& path,                    // (4)
                                                        const string& text,
                                                        permissions p = permissions(0666))
        noexcept;
    async::task<expected<void, error>> async_write_file(const string& path,                    // (5)
                                                        const slice<const byte>& data,
                                                        permissions p = permissions(0666))
        noexcept;
    template<class T>
    async::task<expected<void, error>> async_write_file(const string& path, const T& text,     // (6)
                                                        permissions p = permissions(0666));
}
```

The whole file at `path` written in one call: created or truncated ([create](create.md)), the bytes stored, closed,
the close's error reported too. A file it creates gets the permissions `p`, masked by the umask. Go's
`os.WriteFile`.

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
| `text` | the characters to write |
| `data` | the bytes to write |
| `p` | the permissions of a file it creates, before the umask |

## Return value

Nothing, or the [error](error.md) of the step that failed: of [create](create.md) (its operation `open`:
`is_not_found()` for a directory on the way that is not there, `is_permission()`, `EISDIR` for a directory), of the
write (`ENOSPC` for a full disk) or of the close.

## Complexity

Linear in the size of what is written.

## Exceptions

- (1–3) `std::system_error` when the path names a FIFO or a device, which the file writes on the
  [reactor](../async/readable.md), the write has to wait, and the thread of the reactor, which its first use starts,
  cannot be made.
- (4–5) None: the task's own exceptions are the task's.
- (6) `length_error` when the text is longer than a string holds (4 GiB).

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("state.json", "{\"count\": 3}");
    println("{}", *io::read_text("state.json"));

    string text = "shorter";
    io::write_file("state.json", text);  // truncated first
    println("{}", *io::read_text("state.json"));

    vector<byte> bytes = {byte{0xff}, byte{0x00}};
    io::write_file("data.bin", bytes, io::permissions(0600));
    println("{}", *io::read_file("data.bin"));

    println("{}", io::write_file("no/such/dir/x.txt", "x").error().message());
}
```

Output:

```text
{"count": 3}
shorter
[255, 0]
open no/such/dir/x.txt: No such file or directory
```

A task writes a file without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> save(string path, string text) {
    auto saved = co_await io::async_write_file(path, text);
    println("{} {}", path, static_cast<bool>(saved));
}

int main() {
    async::run(save("saved.txt", "saved by a task"));
    println("{}", *io::read_text("saved.txt"));
}
```

Output:

```text
saved.txt true
saved by a task
```

## See also

- [append_file](append_file.md): the bytes added at the end
- [read_file](read_file.md), [read_text](read_text.md): the other direction
- [create](create.md): a file to write piece by piece
- [sgcl::io::file](file.md)
