[sgcl](../README.md) › [io](README.md)

# sgcl::io::walk_dir, async_walk_dir

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ template<class F>
            expected<void, error> walk_dir(const string& root, F f) noexcept(/* see below */);
    /*(2)*/ template<class F>
            async::task<expected<void, error>> async_walk_dir(const string& root, F f)
                noexcept(std::is_nothrow_move_constructible_v<F>);
}
```

Visits every entry under the directory `root`, Go's `filepath.WalkDir`: in lexical order, a directory before its
contents, `f` called with each as `f(entry, error)`, where the entry is a `const directory_entry&` and the error a
`const optional<error>&`, empty for an entry read well. `f` returns what the walk does next, a
[walk_action](walk_action.md): `next`, `skip_dir` (do not enter this directory), or `stop`. Symbolic links are not
followed: a link to a directory is an entry of type `file_type::symlink`.

`root` itself is not reported. A directory that cannot be read is reported once more after its entry, as an entry of
its path with the error, and the walk goes on unless `f` returns `stop`; when that directory is `root`, it is reported
so as well.

1. The walk on the calling thread. It is `noexcept` when the call of `f` is.
2. The same walk in the same order from a task: each directory is read on the [blocking pool](../async/spawn_blocking.md), and `f` is called in the task,
   between the reads, so that it may touch what the task does.

## Parameters

| Parameter | Description |
|---|---|
| `root` | the directory to walk |
| `f` | what to call with each entry |

## Return value

Nothing when the walk ran, whatever `f` met on the way; or the [error](error.md) of `root`: the error of its `lstat`
(operation `lstat`), or `std::errc::not_a_directory` when it is not a directory (operation `walk_dir`).

## Complexity

Linear in the number of entries under `root`, plus the sort of each directory's listing.

## Exceptions

- (1) What `f` throws.
- (2) None: the task form's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("repo/.git/objects");
    (void)io::mkdir_all("repo/src");
    (void)io::write_file("repo/src/main.cpp", "int main() {}");
    (void)io::write_file("repo/README.md", "# repo");
    uint64_t total = 0;
    auto visit = [&](const io::directory_entry& e, const optional<io::error>& err) {
        if (err) {
            eprintln(err->message());
            return io::walk_action::next;
        }
        if (e.is_directory() && e.name == ".git") {
            return io::walk_action::skip_dir;
        }
        println("{}", e.path);
        if (auto info = e.info()) {
            total += info->is_regular() ? info->size : 0;
        }
        return io::walk_action::next;
    };
    auto walked = io::walk_dir("repo", visit);
    println("{} {} bytes", walked.has_value(), total);
}
```

Output:

```text
repo/README.md
repo/src
repo/src/main.cpp
true 19 bytes
```

Renaming every `*.jpeg` under a directory to `*.jpg`, from a task.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> rename_jpegs(string root) {
    auto rename_one = [](const io::directory_entry& e, const optional<io::error>&) {
        if (io::path::ext(e.path) == ".jpeg") {
            string to = io::path::join(io::path::dir(e.path), io::path::stem(e.path) + ".jpg");
            if (auto r = io::rename(e.path, to)) {
                println("{} -> {}", e.path, io::path::base(to));
            } else {
                eprintln("{}", r.error().message());
            }
        }
        return io::walk_action::next;
    };
    (void)co_await io::async_walk_dir(root, rename_one);
}

int main() {
    (void)io::mkdir_all("photos/2024");
    (void)io::write_file("photos/2024/beach.jpeg", "");
    (void)io::write_file("photos/cat.jpeg", "");
    (void)io::write_file("photos/dog.png", "");
    async::run(rename_jpegs("photos"));
}
```

Output:

```text
photos/2024/beach.jpeg -> beach.jpg
photos/cat.jpeg -> cat.jpg
```

## See also

- [read_dir](read_dir.md): the entries of one directory
- [walk_action](walk_action.md): what `f` returns
- [path::glob](path/glob.md): the paths that match a pattern
