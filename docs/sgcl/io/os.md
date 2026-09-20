# sgcl::io::os — args, getenv, working_dir, stdin

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    vector<string> args();
    optional<string> getenv(const string& name);  result<void> setenv(const string& name, const string& value);  result<void> unsetenv(const string& name);
    vector<pair<string, string>> environ();  string expand_env(const string& s);
    result<string> working_dir();  result<void> chdir(const string& path);
    result<string> home_dir();  result<string> cache_dir();  result<string> config_dir();  string temp_path();
    result<string> executable();  result<string> hostname();  int pid() noexcept;
    tracked_ptr<file> stdin();  tracked_ptr<file> stdout();  tracked_ptr<file> stderr();
    bool is_terminal(int fd) noexcept;
    [[noreturn]] void exit(int code);
}
```

The process and its environment, `os`: the command line, the variables, the directories the platform names, the standard streams as files.

## Rules

- `args()` needs no `main`: the platform keeps the command line (the loader's copy on macOS, `/proc/self/cmdline` on Linux).
- `getenv` distinguishes unset (`nullopt`) from empty; `setenv` and `unsetenv` are the process-wide calls, not thread-safe against a concurrent `getenv` on any platform, as in C.
- `stdin()`, `stdout()`, `stderr()` make a new `file` per call over descriptors 0, 1, 2, which the file never closes; the descriptor's flags are left as they are (a terminal, a redirected file and a pipe from the shell are served by the blocking pool; one made non-blocking by the parent by the reactor). `<cstdio>` defines `stdin`, `stdout` and `stderr` as macros for its `FILE` streams: `os.h` removes them and, where the macro named another variable (macOS), re-binds the C streams under the same names as references in the global scope, so code written for `<stdio.h>` compiles on; a unit with `using namespace sgcl::io` that writes a bare `stderr` for the C stream must qualify one of the two.
- `exit` flushes the C streams and ends the process without running destructors (`_exit`), as Go's `os.Exit`.

## Members

```cpp
vector<string> args();                                   // argv[0] first
optional<string> getenv(const string& name);          // nullopt when unset; "" is a value
result<void> setenv(const string& name, const string& value);
result<void> unsetenv(const string& name);
vector<pair<string, string>> environ();                  // every variable
string expand_env(const string& s);                   // "$NAME" and "${NAME}" replaced, unset ones by ""; a name is letters, digits, '_'
result<string> working_dir();  result<void> chdir(const string& path);
result<string> home_dir();      // $HOME, else the password database
result<string> cache_dir();     // ~/Library/Caches (macOS); $XDG_CACHE_HOME or ~/.cache
result<string> config_dir();    // ~/Library/Application Support (macOS); $XDG_CONFIG_HOME or ~/.config
string temp_path();             // $TMPDIR, else /tmp
result<string> executable();    // the running binary, symlinks resolved
result<string> hostname();
int pid() noexcept;
tracked_ptr<file> stdin();  tracked_ptr<file> stdout();  tracked_ptr<file> stderr();
bool is_terminal(int fd) noexcept;   // isatty
[[noreturn]] void exit(int code);
```

```cpp
auto level = io::getenv("LOG_LEVEL").value_or("info");
auto dir = io::path::join(*io::config_dir(), "myapp");
io::mkdir_all(dir);
auto out = io::stdout();
out->write_text(io::is_terminal(1) ? "\033[1mready\033[0m\n" : "ready\n");
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

namespace io = io;

// cat: the files named, or the standard input, to the standard output
int main() {
    auto args = io::args();
    auto out = io::stdout();
    if (args.size() == 1) {
        if (auto n = io::copy(*out, *io::stdin()); !n) { io::stderr()->write_text(n.error().message() + "\n"); io::exit(1); }
        return 0;
    }
    int status = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        auto f = io::open(args[i]);
        if (!f) { io::stderr()->write_text("cat: " + f.error().message() + "\n"); status = 1; continue; }
        if (auto n = (*f)->copy_to(*out); !n) { io::stderr()->write_text("cat: " + n.error().message() + "\n"); status = 1; }
    }
    return status;
}
```

## See also

- [file](file.md): what the standard streams are; [fs](fs.md), [path](path.md)
- `tests/io/os.cpp`: the environment round trip and `expand_env`, `args`/`executable`/`hostname`/the directories, the standard streams beside the C ones.
