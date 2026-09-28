# sgcl::io::file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    enum class open_flags : unsigned { read = 1, write = 2, create = 4, truncate = 8, append = 16, exclusive = 32, sync = 64 };
    class file final : public mixin::reader<file>, public mixin::writer<file>, public mixin::seeker<file>;   // one class for every descriptor; a handle of one word

    expected<file, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
    expected<file, error> create(const string& path, permissions p = permissions(0666));   // write | create | truncate
    // async_open, async_create: the same for a task, on the blocking pool
    file from_fd(int fd, const string& name = {});
    struct pipe_ends { file read; file write; };
    expected<pipe_ends, error> pipe();

    expected<vector<byte>, error> read_file(const string& path);   expected<string, error> read_text(const string& path);
    expected<void, error> write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));   // and a string's text
    expected<void, error> append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));  // the same
    // async_read_file, async_read_text, async_write_file, async_append_file: the same for a task, on the blocking pool

    expected<file, error> temp_file(const string& dir = {}, const string& pattern = "*");
    expected<string, error> make_temp_dir(const string& dir = {}, const string& pattern = "*");
    // async_temp_file, async_make_temp_dir: the same for a task, on the blocking pool
}
```

A file is one class for every descriptor — a regular file, a pipe, a terminal, later a socket — a [stream](stream.md) with a position. A read or write is the system call on the descriptor. The asynchronous form of an operation goes one of two ways, chosen when the file is made: a regular file's (and a terminal's) runs the call on the [blocking pool](../async/blocking.md), a disk having no readiness to wait for; a non-blocking descriptor's (a pipe from `pipe()`, a socket) waits for readiness on the [reactor](../async/reactor.md) and makes the call when it will not block, and the synchronous form on such a descriptor polls instead. `read_at` and `write_at` are `pread` and `pwrite`: the position given, the file's own untouched, so that tasks share a file without a seek between them.

A `file` is a handle of one word, a `tracked_ptr` to the object inside, as a [`string`](../core/string.md) is: a copy is the same file (one descriptor, one position), passed by value into a task it keeps the file alive for as long as the task runs. `io::file f = io::open(p);` takes the file out of the `expected`, and throws its error when there is none ([expected](../core/expected.md)); `auto opened = io::open(p); if (!opened) ...` is the form that looks at the failure.

## Rules

- Made by `open`, `create`, `temp_file`, `from_fd`, `pipe` and their `async_` forms: a handle, which every function of io takes as a reader, a writer, a closer and a seeker ([stream](stream.md)). A `file` made by its default constructor holds none (`!f`); an operation on it is a contract violation.
- A handle is a tracked word: on a stack, in a task, in a managed object. In a global or a std container it goes into a [`rooted`](../core/rooted.md): `rooted<io::file> log(io::open(p));`, then `log->write(...)` — a copy of the handle in a managed object of its own under a root, the same file. A root is never part of a cycle: a `rooted` never lies in a managed object or in a task's frame, where the handle itself goes.
- A stream made of a file (`io::reader in = f;`) holds the file itself, not the handle: the handle may go first.
- `close()` from any thread or task ends the file: no operation starts after it, a task or a thread waiting in `read` or `write` wakes to `errc::closed`, and the descriptor is given back to the kernel by whoever lets go of it last, `close()` itself or the last operation in progress (a count of the operations and a closing bit in one word, as net's sockets hold theirs), so that no read in progress lands on the number the kernel gives to the next file opened. The descriptor is released by `close()` or, failing that, by the destructor on the collector's thread after the sweep that finds the file dead — later than the last use. A file that is done is closed; `close()` reports what the deferred one could not.
- `from_fd` leaves the descriptor's flags as they are: one that is non-blocking already is served by the reactor, any other by the pool. `open` makes a FIFO or a device non-blocking; `pipe` makes both ends so.
- The data of an async write stays alive while the task awaits, as a task's local does; a `tracked_ptr` to a buffer captured in the awaiting frame is enough.
- One task or thread at a time on the position; `read_at`/`write_at` from any number.

## Members

### open_flags

```cpp
enum class open_flags : unsigned { read = 1, write = 2, create = 4, truncate = 8, append = 16, exclusive = 32, sync = 64 };
constexpr open_flags operator|(open_flags, open_flags) noexcept;
constexpr bool operator&(open_flags, open_flags) noexcept;
```

`read` is the default; `write` alone truncates nothing and creates nothing (an error when the file is not there), so a file to write is `write | create | truncate`, which `create(path)` spells, or `write | create | append` for a log; `exclusive` with `create` fails when the file exists (`O_EXCL`); `sync` makes every write reach the disk before it returns (`O_SYNC`). Every file is `O_CLOEXEC`.

### file

```cpp
file() noexcept;                                                            // no file: !f
expected<size_t, error> read(const slice<byte>& buffer) const;               // 0 at the end
async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const;   // on the reactor (a pipe), or on the blocking pool (a regular file)
expected<size_t, error> write(const slice<const byte>& data) const;          // the whole span
async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const;
expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) const;
expected<void, error> close() const;  bool is_closed() const noexcept;     // a close never waits: no async form
expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset) const;       // pread: the position untouched
expected<size_t, error> write_at(const slice<const byte>& data, uint64_t offset) const; // pwrite
async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer, uint64_t offset) const;
async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data, uint64_t offset) const;
expected<void, error> sync() const;                    // fsync
expected<void, error> truncate(uint64_t size) const;   // ftruncate
async::task<expected<void, error>> async_sync() const;                    // on the blocking pool: an fsync waits for the disk
async::task<expected<void, error>> async_truncate(uint64_t size) const;
expected<file_info, error> stat() const;               // fstat
expected<void, error> chmod(permissions p) const;      // fchmod
async::task<expected<void, error>> async_chmod(permissions p) const;   // on the blocking pool
int fd() const noexcept;                // -1 when closed
const string& path() const noexcept;    // as opened, or the name given to from_fd
bool is_nonblocking() const noexcept;   // served by the reactor rather than the pool
explicit operator bool() const noexcept;                   // whether the handle holds a file
friend bool operator==(const file& a, const file& b) noexcept;   // the same file
```

Plus everything of [`mixin::reader`, `mixin::writer`, `mixin::seeker`](stream.md#members): `read_full`, `read_all`, `read_all_text`, `copy_to`, `write`, `copy_from`, `tell`, `size`, `rewind`, and their `async_` forms.

```cpp
io::file data = io::open("data.bin", io::open_flags::read | io::open_flags::write);
byte header[16];
data.read_full(header);
uint64_t size = data.size();               // the end, the position kept
data.write_at(footer, size - 8);           // no seek: the position is still after the header
data.sync();
data.close();
```

### open, create, from_fd, pipe

```cpp
expected<file, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
expected<file, error> create(const string& path, permissions p = permissions(0666));
file from_fd(int fd, const string& name = {});   // owns the descriptor from now on
expected<pipe_ends, error> pipe();               // ends.read, ends.write (or auto [r, w] = ends), both non-blocking
async::task<expected<file, error>> async_open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
async::task<expected<file, error>> async_create(const string& path, permissions p = permissions(0666));
```

`open`'s error answers `is_not_found()`, `is_permission()`, `is_exists()` (with `exclusive`). `p` is masked by the umask, as `open(2)` does. `co_await io::async_open(...)` makes the same call on the blocking pool: an open waits for the disk, and one of a FIFO for the other end.

```cpp
io::file log = io::open("app.log", io::open_flags::write | io::open_flags::create | io::open_flags::append, io::permissions(0644));
auto lock = io::open("app.lock", io::open_flags::write | io::open_flags::create | io::open_flags::exclusive);
if (!lock && lock.error().is_exists()) { /* another instance runs */ }
```

### read_file, read_text, write_file, append_file

```cpp
expected<vector<byte>, error> read_file(const string& path);   // the size from fstat, one buffer, one read (and more, should the file have grown)
expected<string, error> read_text(const string& path);
expected<void, error> write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));    // created or truncated, written, closed
expected<void, error> write_file(const string& path, const string& text, permissions p = permissions(0666));
expected<void, error> append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));   // at the end, created when missing
expected<void, error> append_file(const string& path, const string& text, permissions p = permissions(0666));
async::task<expected<vector<byte>, error>> async_read_file(const string& path);   // and async_read_text, async_write_file, async_append_file
```

Each does its work on the calling thread; `co_await io::async_read_file(p)` in a task runs the same on the blocking pool (a disk has no readiness to wait for).

```cpp
auto config = io::read_text("app.toml").value_or("");
io::write_file("state.json", state.dump());
io::append_file("app.log", "started\n");
```

### temp_file, make_temp_dir

```cpp
expected<file, error> temp_file(const string& dir = {}, const string& pattern = "*");   // 0600, read and write
expected<string, error> make_temp_dir(const string& dir = {}, const string& pattern = "*");          // 0700
async::task<expected<file, error>> async_temp_file(const string& dir = {}, const string& pattern = "*");   // on the blocking pool
async::task<expected<string, error>> async_make_temp_dir(const string& dir = {}, const string& pattern = "*");
```

A new file or directory in `dir` (the system's temporary directory when empty: `$TMPDIR`, else `/tmp`) with a name from the pattern, its last `*` replaced by ten random characters (`"upload-*.tmp"`; appended when there is none). The caller removes it.

```cpp
string dir = io::make_temp_dir({}, "build-*");
io::file object = io::temp_file(dir, "*.o");
/* ... */
io::remove_all(dir);
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

// Copies a file through the pool without holding a worker, then reads it
// back through a pipe; the failures returned, not thrown
async::task<expected<size_t, io::error>> roundtrip(string src, string dst) {   // by value: a task copies its parameters into its frame
    auto in = co_await io::async_open(src);
    if (!in) co_return unexpected(in.error());
    auto out = co_await io::async_create(dst);
    if (!out) co_return unexpected(out.error());
    auto n = co_await in->async_copy_to(*out);             // reads and writes on the blocking pool
    if (!n) co_return n;
    out->close();                                           // a file's close never waits

    auto ends = io::pipe();                                 // a pipe: both ends on the reactor
    if (!ends) co_return unexpected(ends.error());
    async::go([sending = ends->write, dst]() -> async::task<> {
        auto data = co_await io::async_read_file(dst);
        if (data) co_await sending.async_write(data->as_slice());
        sending.close();                                    // the reader sees the end
    });
    auto back = co_await ends->read.async_read_all();       // suspended until the writer is done
    if (!back) co_return unexpected(back.error());
    co_return back->size();
}

int main() {
    auto n = async::spawn(roundtrip("/etc/hosts", "/tmp/hosts.copy")).wait();
    if (!n) {
        eprintln(n.error().message());
        return 1;
    }
    println("{} bytes", *n);
    io::remove("/tmp/hosts.copy");
}
```

## See also

- [stream](stream.md): the interfaces and the mixins; [buffered](buffered.md): lines over a file; [fs](fs.md): `file_info`, `permissions`, the directory operations
- [blocking](../async/blocking.md), [reactor](../async/reactor.md): where the async forms wait
- `tests/io/rooted.cpp`: `rooted<io::file>` from `open` and from a handle (a copy of it, the same state), from an `expected` holding an error (the exception), `rooted<io::buffered_reader>` made in place, `rooted<io::buffer>` in a `std::vector` and in a global through collections.
- `tests/io/file.cpp`: create/write/read, the flags, `seek`/`read_at`/`truncate`/`stat`, a buffered file, `temp_file`, a pipe read by polling, the pool and the reactor from a task, a writer blocked by a full pipe, a file closed through an `io::reader` and through a `buffered_reader`, the handle (copies, a stream of a temporary, a `root_ptr` to it).
