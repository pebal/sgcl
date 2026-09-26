# sgcl::io::fs — stat, mkdir, remove, rename, read_dir, walk_dir

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    enum class permissions : unsigned;      // rwxrwxrwx and the set-id and sticky bits: permissions(0644)
    enum class file_type { unknown, regular, directory, symlink, block, character, fifo, socket };
    using file_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
    struct file_info;                       // name, size, type, mode, modified
    struct directory_entry;                       // name, path, type; info() on demand
    enum class walk_action { next, skip_dir, stop };

    expected<file_info, error> stat(const string& path);   expected<file_info, error> lstat(const string& path);
    bool exists(const string& path) noexcept;  bool is_directory(const string& path) noexcept;  bool is_regular(const string& path) noexcept;
    expected<void, error> mkdir(const string& path, permissions p = permissions(0777));
    expected<void, error> mkdir_all(const string& path, permissions p = permissions(0777));
    expected<void, error> remove(const string& path);   expected<void, error> remove_all(const string& path);
    expected<void, error> rename(const string& from, const string& to);
    expected<void, error> copy_file(const string& from, const string& to);
    expected<void, error> symlink(const string& target, const string& link);   expected<string, error> read_link(const string& link);
    expected<void, error> chmod(const string& path, permissions p);   expected<void, error> set_modified(const string& path, file_time t);
    expected<vector<directory_entry>, error> read_dir(const string& path);   async::task<expected<vector<directory_entry>, error>> async_read_dir(const string& path);
    template<class F> expected<void, error> walk_dir(const string& root, F f);
    // async_stat, async_lstat, async_mkdir_all, async_remove_all, async_copy_file, async_read_dir, async_walk_dir: the same for a task, on the blocking pool
}
```

The file system: what is at a path, and making, moving and removing things there. Paths are strings; the operations are `std::filesystem`'s and the platform's under a thin layer that returns [`expected<T, error>`](error.md) and speaks in this module's types. A function that takes a path follows symlinks, its `l`-variant does not, as on POSIX. The names of `os` in Go: `Stat`, `Mkdir`, `MkdirAll`, `Remove`, `RemoveAll`, `Rename`, `ReadDir`, `WalkDir`.

## Rules

- Every function returns `expected<T, error>`; the three questions (`exists`, `is_directory`, `is_regular`) return `false` on any error, being the shortcut, and `stat()` is the answer with the error.
- `mkdir_all` and `remove_all` are idempotent: an existing directory, a missing path, are no error.
- `rename` replaces what is at the new path (`rename(2)`); `copy_file` replaces the target.
- `walk_dir` does not follow symlinks and reports a directory it cannot read once, with the error, and goes on.
- The operations that wait for the disk as a read does have an `async_` form, which runs the same call on the [blocking pool](../async/blocking.md) so that a task holds no worker while the disk works: `stat`, `lstat`, `mkdir_all`, `remove_all`, `copy_file`, `read_dir`, `walk_dir` (and in [file](file.md) `open`, `create`, `temp_file`, `make_temp_dir`, `sync`, `truncate` and the whole-file functions). The others — `mkdir`, `remove`, `rename`, `symlink`, `read_link`, `chmod`, `set_modified` — are one quick system call and have none; `exists`, `is_directory` and `is_regular` are the shortcuts of a stat, and a task asks `co_await async_stat(p)` instead.

## Members

### permissions, file_type, file_info, directory_entry

```cpp
enum class permissions : unsigned {
    none = 0, owner_read = 0400, owner_write = 0200, owner_exec = 0100, group_read = 040, group_write = 020, group_exec = 010,
    others_read = 04, others_write = 02, others_exec = 01, set_uid = 04000, set_gid = 02000, sticky = 01000, all = 0777
};
constexpr permissions operator|(permissions, permissions) noexcept;  constexpr permissions operator&(permissions, permissions) noexcept;

struct file_info {
    string name;  uint64_t size;  file_type type;  permissions mode;  file_time modified;
    bool is_regular() const noexcept;  bool is_directory() const noexcept;  bool is_symlink() const noexcept;
};

struct directory_entry {
    string name;  string path;  file_type type;         // the type from the listing itself (d_type), no stat
    bool is_directory() const noexcept;
    expected<file_info, error> info() const;                     // lstat when asked
};
```

```cpp
auto info = io::stat("photo.jpg");
if (info && info->is_regular() && info->size > 10 << 20) ...
if (static_cast<unsigned>(info->mode & io::permissions::others_write)) std::cerr << "world-writable\n";
```

### stat, lstat, exists, is_directory, is_regular

```cpp
expected<file_info, error> stat(const string& path);    // follows a symlink
expected<file_info, error> lstat(const string& path);   // the symlink itself
bool exists(const string& path) noexcept;
bool is_directory(const string& path) noexcept;
bool is_regular(const string& path) noexcept;
async::task<expected<file_info, error>> async_stat(const string& path);    // on the blocking pool
async::task<expected<file_info, error>> async_lstat(const string& path);
```

### mkdir, mkdir_all, remove, remove_all, rename, copy_file

```cpp
expected<void, error> mkdir(const string& path, permissions p = permissions(0777));       // one directory: an error when the parent is missing or it exists
expected<void, error> mkdir_all(const string& path, permissions p = permissions(0777));   // the whole chain, existing ones left alone (mkdir -p)
expected<void, error> remove(const string& path);        // a file, a symlink or an empty directory; missing is not found
expected<void, error> remove_all(const string& path);    // everything under the path and the path; missing is no error
expected<void, error> rename(const string& from, const string& to);
expected<void, error> copy_file(const string& from, const string& to);   // the bytes and the permissions of a regular file
async::task<expected<void, error>> async_mkdir_all(const string& path, permissions p = permissions(0777));   // on the blocking pool
async::task<expected<void, error>> async_remove_all(const string& path);
async::task<expected<void, error>> async_copy_file(const string& from, const string& to);
```

```cpp
auto cache = io::path::join(*io::cache_dir(), "myapp");
io::mkdir_all(cache);
io::write_file(io::path::join(cache, "index.tmp"), data);
io::rename(io::path::join(cache, "index.tmp"), io::path::join(cache, "index"));   // atomic replace
```

### symlink, read_link, chmod, set_modified

```cpp
expected<void, error> symlink(const string& target, const string& link);
expected<string, error> read_link(const string& link);
expected<void, error> chmod(const string& path, permissions p);
expected<void, error> set_modified(const string& path, file_time t);   // utimensat; the access time untouched
```

### read_dir, walk_dir

```cpp
expected<vector<directory_entry>, error> read_dir(const string& path);              // sorted by name, "." and ".." left out
async::task<expected<vector<directory_entry>, error>> async_read_dir(const string& path);  // on the blocking pool
template<class F> expected<void, error> walk_dir(const string& root, F f);   // F: walk_action(const directory_entry&, const optional<error>&)
template<class F> async::task<expected<void, error>> async_walk_dir(const string& root, F f);   // each directory read on the pool, f called in the task
```

`walk_dir` visits every entry under `root` in lexical order, the directory before its contents, and calls `f` with each; `f` returns `walk_action::next`, `skip_dir` (do not enter this directory) or `stop`. A directory that cannot be read is reported once as its entry with the error, and the walk goes on; `root` itself is not reported; an error of `root` is the result. `async_walk_dir` makes the same walk in the same order from a task: each directory is read on the blocking pool, and `f` is called in the task, between the reads, so that it may touch what the task does.

```cpp
uint64_t total = 0;
io::walk_dir(".", [&](const io::directory_entry& e, const optional<io::error>& err) {
    if (err) { std::cerr << err->message() << '\n'; return io::walk_action::next; }
    if (e.is_directory() && e.name == ".git") return io::walk_action::skip_dir;
    if (auto i = e.info()) total += i->size;
    return io::walk_action::next;
});
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// Removes the files of a directory older than a week
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    auto entries = io::read_dir(argv[1]);
    if (!entries) { std::cerr << entries.error().message() << '\n'; return 1; }
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
    for (auto& e : *entries) {
        auto info = e.info();
        if (info && info->is_regular() && info->modified < cutoff) {
            if (auto r = io::remove(e.path); !r) std::cerr << r.error().message() << '\n';
            else std::cout << "removed " << e.name << '\n';
        }
    }
}
```

## See also

- [file](file.md): `file::stat`, `temp_dir`; [path](path.md): the lexical side; [os](os.md): the directories the platform names
- `tests/io/fs.cpp`: mkdir/remove/rename, stat/lstat/symlink/chmod/set_modified, a sorted listing with types, the walk's order, skip and stop; every `async_` form in one task, and the task's walk skipping and stopping as `walk_dir` does.
