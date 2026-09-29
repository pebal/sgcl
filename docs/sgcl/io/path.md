# sgcl::io::path

```cpp
#include "sgcl/io/path.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io::path {
    inline constexpr char separator = '/';  inline constexpr char list_separator = ':';
    string clean(const string& p);
    string join(std::initializer_list<string> elements);   template<class R> string join(const R& elements);   template<class... S> string join(const string& first, const S&... rest);
    string base(const string& p);  string dir(const string& p);  string ext(const string& p);  string stem(const string& p);
    pair<string, string> split(const string& p);  vector<string> split_list(const string& list);
    bool is_abs(const string& p) noexcept;
    bool is_local(const string& name) noexcept;  expected<string, error> under(const string& dir, const string& name);
    expected<string, error> abs(const string& p);  expected<string, error> rel(const string& base, const string& target);
    expected<bool, error> match(const string& pattern, const string& name);  expected<vector<string>, error> glob(const string& pattern);
    string from_slash(const string& p);  string to_slash(const string& p);
}
```

Paths as strings, `path/filepath`: lexical operations on the text of a path in the platform's form (`/` on POSIX), touching the file system only where the name says so (`abs`, `glob`). Nothing here allocates a path type: a [`string`](../core/string.md) in (a literal makes one), a `string` out. Text is UTF-8: `match` compares code points, so `?` is one character and `[α-ω]` a range of them.

## Rules

- `clean` is applied by `join`, `dir`, `abs`, `rel`; `base`, `ext`, `split` work on the text as given.
- `match` matches the whole name, element by element: `*` and `?` never match a separator.
- A malformed pattern (an unclosed `[`, a trailing `\`, a reversed range) is `errc::invalid_pattern`, whatever the name.
- **A name from outside** — an entry of an archive, the path of a request (which may have been `..%2f` before it was decoded), a name a user typed — goes through `under(dir, name)` before it becomes a file's path: the name joined to the directory when `is_local(name)`, `errc::insecure_path` when it is not, nothing joined. `is_local` is Go's `filepath.IsLocal`: not empty, not absolute, no NUL, no `\`, and no `..` that climbs above the start once the name is taken lexically (`a/../b` is local, `a/../..` is not). The archives' extraction ([`tar`](../compress/tar.md), [`zip`](../compress/zip.md), [`sevenzip`](../compress/sevenzip.md)) holds its names to the same rule.

## Members

```cpp
string clean(const string& p);           // the shortest equivalent: "." and ".." resolved, repeated and trailing separators dropped; "" is "."
string join(std::initializer_list<string> elements);   // with the separator, cleaned; empty elements skipped; all empty gives ""
template<class R> string join(const R& elements);      // the same over a range of strings
template<class... S> string join(const string& first, S... rest);
string base(const string& p);            // the last element; "" and "/" give themselves
string dir(const string& p);             // everything but the last element, cleaned; "." when none
string ext(const string& p);             // from the last dot of the last element, "" when none (".bashrc" is all extension)
string stem(const string& p);            // base without ext
pair<string, string> split(const string& p);       // {dir with its trailing separator as written, file}
vector<string> split_list(const string& list);     // a PATH-like list, empty elements skipped
bool is_abs(const string& p) noexcept;
bool is_local(const string& name) noexcept;                 // joined to a directory, it stays inside: Go's filepath.IsLocal
expected<string, error> under(const string& dir, const string& name);   // dir joined with name, cleaned, when it is local; errc::insecure_path when not
expected<string, error> abs(const string& p);               // the working directory joined when relative, cleaned
expected<string, error> rel(const string& base, const string& target);   // the path from base to target with ".."; errc::invalid_path when one is absolute and the other not, or base begins with ".."
expected<bool, error> match(const string& pattern, const string& name);  // '*' any run without a separator, '?' one character, '[a-z]' a class, '[^a-z]' its negation, '\' an escape
expected<vector<string>, error> glob(const string& pattern);                // the paths that match, sorted within each directory; unreadable directories skipped; without meta characters, the file if it exists
string from_slash(const string& p);  string to_slash(const string& p);   // identity on POSIX
```

```cpp
io::path::clean("a//b/../c/");                  // "a/c"
io::path::join("/usr", "local", "bin");         // "/usr/local/bin"
io::path::base("/a/b/c.tar.gz");                // "c.tar.gz"
io::path::dir("/a/b/c.tar.gz");                 // "/a/b"
io::path::ext("/a/b/c.tar.gz");                 // ".gz"
io::path::stem("/a/b/c.tar.gz");                // "c.tar"
*io::path::rel("/a/b", "/a/c/d");               // "../c/d"
*io::path::match("*.cpp", "main.cpp");          // true
*io::path::match("src/*.cpp", "src/a/b.cpp");   // false: '*' stops at '/'
*io::path::match("[а-я]*", "яблоко");           // true: a range of code points
for (auto& p : *io::path::glob("tests/*/*.cpp")) ...
```

## Example

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    for (const char* name : {"docs/a.txt", "a/../b.txt", "../secret.txt", "/etc/passwd"}) {
        auto file = io::path::under("public", name);
        println("{} -> {}", name, file ? *file : file.error().message());
    }
}
```

Output:

```text
docs/a.txt -> public/docs/a.txt
a/../b.txt -> public/b.txt
../secret.txt -> under ../secret.txt: insecure path
/etc/passwd -> under /etc/passwd: insecure path
```

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

// Renames every *.jpeg under a directory to *.jpg
int main(int argc, char** argv) {
    auto root = argc > 1 ? argv[1] : ".";
    io::walk_dir(root, [](const io::directory_entry& e, const optional<io::error>&) {
        if (io::path::ext(e.path) == ".jpeg") {
            auto to = io::path::join(io::path::dir(e.path), io::path::stem(e.path) + ".jpg");
            if (auto r = io::rename(e.path, to)) println("{} -> {}", e.path, io::path::base(to));
            else eprintln("{}", r.error().message());
        }
        return io::walk_action::next;
    });
}
```

## See also

- [fs](fs.md): what is at the path; [os](os.md): `working_dir`, the directories the platform names
- `tests/io/path.cpp`: Go's table for `IsLocal` with `under`, for `Clean`, `join`/`base`/`dir`/`ext`/`stem`/`split`, `abs`/`rel`, `match` with classes, escapes and code points, `glob` over a tree.
