# sgcl::io::file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    enum class open_flags : unsigned { read = 1, write = 2, create = 4, truncate = 8, append = 16, exclusive = 32, sync = 64 };
    class file final : public mixin::reader<file>, public mixin::writer<file>, public mixin::seeker<file>;   // one class for every descriptor

    expected<tracked_ptr<file>, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
    expected<tracked_ptr<file>, error> create(const string& path, permissions p = permissions(0666));   // write | create | truncate
    // async_open, async_create: the same for a task, on the blocking pool
    tracked_ptr<file> from_fd(int fd, const string& name = {});
    struct pipe_ends { tracked_ptr<file> read; tracked_ptr<file> write; };
    expected<pipe_ends, error> pipe();

    expected<vector<byte>, error> read_file(const string& path);   expected<string, error> read_text(const string& path);
    expected<void, error> write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));   // and a string's text
    expected<void, error> append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666));  // the same
    // async_read_file, async_read_text, async_write_file, async_append_file: the same for a task, on the blocking pool

    expected<tracked_ptr<file>, error> temp_file(const string& dir = {}, const string& pattern = "*");
    expected<string, error> make_temp_dir(const string& dir = {}, const string& pattern = "*");
    // async_temp_file, async_make_temp_dir: the same for a task, on the blocking pool
}
```

A file is one class for every descriptor — a regular file, a pipe, a terminal, later a socket — a [stream](stream.md) with a position. A read or write is the system call on the descriptor. The asynchronous form of an operation goes one of two ways, chosen when the file is made: a regular file's (and a terminal's) runs the call on the [blocking pool](../async/blocking.md), a disk having no readiness to wait for; a non-blocking descriptor's (a pipe from `pipe()`, a socket) waits for readiness on the [reactor](../async/reactor.md) and makes the call when it will not block, and the synchronous form on such a descriptor polls instead. `read_at` and `write_at` are `pread` and `pwrite`: the position given, the file's own untouched, so that tasks share a file without a seek between them.

## Rules

- Made by `open`, `create`, `from_fd`, `pipe` (the constructor is private): a `tracked_ptr<file>`, which every function of io takes as a reader, a writer, a closer and a seeker ([stream](stream.md)).
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
expected<size_t, error> read(const slice<byte>& buffer);                     // 0 at the end
async::task<expected<size_t, error>> async_read(slice<byte> buffer);  // on the reactor (a pipe), or on the blocking pool (a regular file)
expected<size_t, error> write(const slice<const byte>& data);                // the whole span
async::task<expected<size_t, error>> async_write(slice<const byte> data);
expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin);
expected<void, error> close();  bool is_closed() const noexcept;           // a close never waits: no async form
expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset);            // pread: the position untouched
expected<size_t, error> write_at(const slice<const byte>& data, uint64_t offset);      // pwrite
async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer, uint64_t offset);
async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data, uint64_t offset);
expected<void, error> sync();                    // fsync
expected<void, error> truncate(uint64_t size);   // ftruncate
async::task<expected<void, error>> async_sync();                    // on the blocking pool: an fsync waits for the disk
async::task<expected<void, error>> async_truncate(uint64_t size);
expected<file_info, error> stat() const;         // fstat
expected<void, error> chmod(permissions p);      // fchmod
int fd() const noexcept;                // -1 when closed
const string& path() const noexcept; // as opened, or the name given to from_fd
bool is_nonblocking() const noexcept;   // served by the reactor rather than the pool
```

Plus everything of [`mixin::reader`, `mixin::writer`, `mixin::seeker`](stream.md#members): `read_full`, `read_all`, `read_all_text`, `copy_to`, `write`, `copy_from`, `tell`, `size`, `rewind`, and their `async_` forms.

```cpp
auto f = io::open("data.bin", io::open_flags::read | io::open_flags::write);
if (!f) return;
byte header[16];
(*f)->read_full(header);
auto size = (*f)->size();                  // the end, the position kept
(*f)->write_at(footer, *size - 8);       // no seek: the position is still after the header
(*f)->sync();
(*f)->close();
```

### open, create, from_fd, pipe

```cpp
expected<tracked_ptr<file>, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
expected<tracked_ptr<file>, error> create(const string& path, permissions p = permissions(0666));
tracked_ptr<file> from_fd(int fd, const string& name = {});   // owns the descriptor from now on
expected<pipe_ends, error> pipe();       // p->read, p->write (or auto [r, w] = *p), both non-blocking
async::task<expected<tracked_ptr<file>, error>> async_open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666));
async::task<expected<tracked_ptr<file>, error>> async_create(const string& path, permissions p = permissions(0666));
```

`open`'s error answers `is_not_found()`, `is_permission()`, `is_exists()` (with `exclusive`). `p` is masked by the umask, as `open(2)` does. `co_await io::async_open(...)` makes the same call on the blocking pool: an open waits for the disk, and one of a FIFO for the other end.

```cpp
auto log = io::open("app.log", io::open_flags::write | io::open_flags::create | io::open_flags::append, io::permissions(0644));
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
expected<tracked_ptr<file>, error> temp_file(const string& dir = {}, const string& pattern = "*");   // 0600, read and write
expected<string, error> make_temp_dir(const string& dir = {}, const string& pattern = "*");          // 0700
async::task<expected<tracked_ptr<file>, error>> async_temp_file(const string& dir = {}, const string& pattern = "*");   // on the blocking pool
async::task<expected<string, error>> async_make_temp_dir(const string& dir = {}, const string& pattern = "*");
```

A new file or directory in `dir` (the system's temporary directory when empty: `$TMPDIR`, else `/tmp`) with a name from the pattern, its last `*` replaced by ten random characters (`"upload-*.tmp"`; appended when there is none). The caller removes it.

```cpp
auto dir = io::make_temp_dir({}, "build-*");
auto obj = io::temp_file(*dir, "*.o");
/* ... */
io::remove_all(*dir);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// Copies a file through the pool without holding a worker, then reads it back through a pipe
async::task<expected<size_t, io::error>> roundtrip(string src, string dst) {   // by value: a task copies its parameters into its frame
    auto in = io::open(src);
    if (!in) co_return unexpected(in.error());
    auto out = io::create(dst);
    if (!out) co_return unexpected(out.error());
    auto n = co_await (*in)->async_copy_to(**out);     // reads and writes on the blocking pool
    if (!n) co_return n;
    (*out)->close();                              // a file's close never waits

    auto p = io::pipe();                                // a pipe: both ends on the reactor
    if (!p) co_return unexpected(p.error());
    auto [rd, wr] = *p;
    async::go([wr, dst]() -> async::task<> {
        auto data = co_await io::async_read_file(dst);
        if (data) co_await wr->async_write(*data);
        wr->close();                              // the reader sees the end
    });
    auto back = co_await rd->async_read_all();          // suspended until the writer is done
    co_return back ? expected<size_t, io::error>(back->size()) : unexpected(back.error());
}

int main() {
    auto n = async::spawn(roundtrip("/etc/hosts", "/tmp/hosts.copy")).wait();
    if (!n) { std::cerr << n.error().message() << '\n'; return 1; }
    std::cout << *n << " bytes\n";
    io::remove("/tmp/hosts.copy");
}
```

## See also

- [stream](stream.md): the interfaces and the mixins; [buffered](buffered.md): lines over a file; [fs](fs.md): `file_info`, `permissions`, the directory operations
- [blocking](../async/blocking.md), [reactor](../async/reactor.md): where the async forms wait
- `tests/io/file.cpp`: create/write/read, the flags, `seek`/`read_at`/`truncate`/`stat`, a buffered file, `temp_file`, a pipe read by polling, the pool and the reactor from a task, a writer blocked by a full pipe, a file closed through an `io::reader` and through a `buffered_reader`.
