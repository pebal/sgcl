# sgcl::io::fs — stat, mkdir, remove, rename, read_dir, walk_dir

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    enum class permissions : unsigned;      // rwxrwxrwx and the set-id and sticky bits: permissions(0644)
    enum class file_type { unknown, regular, directory, symlink, block, character, fifo, socket };
    using file_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
    struct file_info;                       // name, size, type, mode, modified
    struct dir_entry;                       // name, path, type; info() on demand
    enum class walk_action { next, skip_dir, stop };

    result<file_info> stat(const string& path);   result<file_info> lstat(const string& path);
    bool exists(const string& path) noexcept;  bool is_directory(const string& path) noexcept;  bool is_regular(const string& path) noexcept;
    result<void> mkdir(const string& path, permissions p = permissions(0777));
    result<void> mkdir_all(const string& path, permissions p = permissions(0777));
    result<void> remove(const string& path);   result<void> remove_all(const string& path);
    result<void> rename(const string& from, const string& to);
    result<void> copy_file(const string& from, const string& to);
    result<void> symlink(const string& target, const string& link);   result<string> read_link(const string& link);
    result<void> chmod(const string& path, permissions p);   result<void> set_modified(const string& path, file_time t);
    result<vector<dir_entry>> read_dir(const string& path);   task<result<vector<dir_entry>>> async_read_dir(const string& path);
    template<class F> result<void> walk_dir(const string& root, F f);
}
```

The file system: what is at a path, and making, moving and removing things there. Paths are strings; the operations are `std::filesystem`'s and the platform's under a thin layer that returns [`result<T>`](error.md) and speaks in this module's types. A function that takes a path follows symlinks, its `l`-variant does not, as on POSIX. The names of `os` in Go: `Stat`, `Mkdir`, `MkdirAll`, `Remove`, `RemoveAll`, `Rename`, `ReadDir`, `WalkDir`.

## Rules

- Every function returns `result<T>`; the three questions (`exists`, `is_directory`, `is_regular`) return `false` on any error, being the shortcut, and `stat()` is the answer with the error.
- `mkdir_all` and `remove_all` are idempotent: an existing directory, a missing path, are no error.
- `rename` replaces what is at the new path (`rename(2)`); `copy_file` replaces the target.
- `walk_dir` does not follow symlinks and reports a directory it cannot read once, with the error, and goes on.

## Members

### permissions, file_type, file_info, dir_entry

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

struct dir_entry {
    string name;  string path;  file_type type;         // the type from the listing itself (d_type), no stat
    bool is_directory() const noexcept;
    result<file_info> info() const;                     // lstat when asked
};
```

```cpp
auto info = io::stat("photo.jpg");
if (info && info->is_regular() && info->size > 10 << 20) ...
if (static_cast<unsigned>(info->mode & io::permissions::others_write)) std::cerr << "world-writable\n";
```

### stat, lstat, exists, is_directory, is_regular

```cpp
result<file_info> stat(const string& path);    // follows a symlink
result<file_info> lstat(const string& path);   // the symlink itself
bool exists(const string& path) noexcept;
bool is_directory(const string& path) noexcept;
bool is_regular(const string& path) noexcept;
```

### mkdir, mkdir_all, remove, remove_all, rename, copy_file

```cpp
result<void> mkdir(const string& path, permissions p = permissions(0777));       // one directory: an error when the parent is missing or it exists
result<void> mkdir_all(const string& path, permissions p = permissions(0777));   // the whole chain, existing ones left alone (mkdir -p)
result<void> remove(const string& path);        // a file, a symlink or an empty directory; missing is not found
result<void> remove_all(const string& path);    // everything under the path and the path; missing is no error
result<void> rename(const string& from, const string& to);
result<void> copy_file(const string& from, const string& to);   // the bytes and the permissions of a regular file
```

```cpp
auto cache = io::path::join(*io::cache_dir(), "myapp");
io::mkdir_all(cache);
io::write_file(io::path::join(cache, "index.tmp"), data);
io::rename(io::path::join(cache, "index.tmp"), io::path::join(cache, "index"));   // atomic replace
```

### symlink, read_link, chmod, set_modified

```cpp
result<void> symlink(const string& target, const string& link);
result<string> read_link(const string& link);
result<void> chmod(const string& path, permissions p);
result<void> set_modified(const string& path, file_time t);   // utimensat; the access time untouched
```

### read_dir, walk_dir

```cpp
result<vector<dir_entry>> read_dir(const string& path);              // sorted by name, "." and ".." left out
task<result<vector<dir_entry>>> async_read_dir(const string& path);  // on the blocking pool
template<class F> result<void> walk_dir(const string& root, F f);   // F: walk_action(const dir_entry&, const optional<error>&)
```

`walk_dir` visits every entry under `root` in lexical order, the directory before its contents, and calls `f` with each; `f` returns `walk_action::next`, `skip_dir` (do not enter this directory) or `stop`. A directory that cannot be read is reported once as its entry with the error, and the walk goes on; `root` itself is not reported; an error of `root` is the result.

```cpp
uint64_t total = 0;
io::walk_dir(".", [&](const io::dir_entry& e, const optional<io::error>& err) {
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

namespace io = io;

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
- `tests/io/fs.cpp`: mkdir/remove/rename, stat/lstat/symlink/chmod/set_modified, a sorted listing with types, the walk's order, skip and stop.
