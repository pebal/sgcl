# sgcl::io::print, println, eprint, eprintln

```cpp
#include "sgcl/io/print.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    void print(pattern, const A&... args);                        // on io::stdout
    void println(pattern, const A&... args);                      // and a new line
    void println();                                               // a new line alone
    void print(const T& value);  void println(const T& value);    // one value, as "{}" formats it
    void eprint(pattern, const A&... args);  void eprintln(pattern, const A&... args);   // on io::stderr
    void eprint(const T& value);  void eprintln(const T& value);  void eprintln();
    void print(const io::writer& to, pattern, const A&... args);  // on any stream: a file, a connection, a buffer
    void println(const io::writer& to, pattern, const A&... args);
    bool print(const txt::runtime_pattern& pattern, const A&... args);    // a pattern read at run time: printed when it fits
    bool println(const txt::runtime_pattern& pattern, const A&... args);
}
namespace sgcl {
    using io::print;  using io::println;  using io::eprint;  using io::eprintln;
}
```

Text written in one call: `println("{} items", n)` is `io::stdout.write(txt::format("{} items\n", n))`. The pattern is [`txt::format`](../txt/format.md)'s and the compiler reads it: a brace left open, a value that does not take its field is an error of the build. The names are `sgcl`'s as well as `io`'s, so `using namespace sgcl;` is all a program needs.

## Rules

- **One value needs no pattern**: `println(n)`, `println(name)`, `println(when)` — anything `"{}"` formats, text included. A literal is always the pattern: `println("done")` prints `done`, and `println("{}")` does not compile (a field with no value), so a literal with braces to be printed as they are is `println("{}", "{}")`.
- **Printing is not checked.** A line on a terminal that could not be written has nobody to tell, and a result every call would have to drop is noise, so the functions return nothing (the runtime forms say only whether the pattern fitted). A program that must know — a pipe closed under it — writes with `io::stdout.write(...)`, which answers an [`expected`](../core/expected.md).
- **The streams stay what they are.** [`io::stdout`, `io::stderr`](os.md) are streams: bytes, `io::copy(io::stdout, file)`, a [`buffered_writer`](buffered.md) over them, `co_await io::stdout.async_write(...)` in a task. `print` is only the short way to put text on them, or on any other writer: `println(file, "{} {}", key, value)`.
- **A pattern from data** — a translation read from a file — goes through `txt::runtime(entry)`; when it does not fit its values (`{0} {1}` where one is given) nothing is printed and the call returns `false`, so a program falls back on its own pattern: `println(txt::runtime(entry), n) || println("{} left", n);`.
- In a task, `print` writes as `io::stdout.write` does, on the calling worker; a program that prints much from tasks writes through a buffered writer.

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

int main() {
    auto files = io::read_dir(".");
    if (!files) {
        eprintln("cannot list the directory: {}", files.error().message());
        return 1;
    }
    println("{} entries", files->size());
    for (auto& e : *files) {
        println("  {:<20} {}", e.name, e.is_directory() ? "dir" : "file");
    }
    println();
    println(time::now());
}
```

## See also

[`txt::format`](../txt/format.md) (the patterns), [`os`](os.md) (`io::stdout`, `io::stderr`), [`stream`](stream.md) (writers).
