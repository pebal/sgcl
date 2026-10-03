[sgcl](../README.md) › [io](README.md)

# sgcl::io::args

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    vector<string> args() noexcept;
}
```

Returns the command line of the process, the program's name first and then its arguments, as Go's `os.Args`.
No `main` is needed to reach it: the platform keeps the command line, the loader's copy on macOS (`_NSGetArgv`) and
`/proc/self/cmdline` on Linux, so a library, a static initializer or a task asks for it where it is.

## Parameters

None.

## Return value

The arguments, `argv[0]` first, each a [string](../core/string/README.md). Empty when the platform's copy cannot be read.

## Complexity

Linear in the length of the command line: a string is made of each argument at every call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> all = io::args();
    println("{} argument(s) after the program's name", all.size() - 1);
}
```

Output:

```text
0 argument(s) after the program's name
```

`cat`: the files named, or the standard input, to the standard output.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto args = io::args();
    if (args.size() == 1) {
        if (auto n = io::copy(io::stdout, io::stdin); !n) {
            eprintln(n.error().message());
            io::exit(1);
        }
        return 0;
    }
    int status = 0;
    for (const string& name : args.as_slice(1)) {
        auto f = io::open(name);
        if (!f) {
            eprintln("cat: {}", f.error().message());
            status = 1;
            continue;
        }
        if (auto n = io::copy(io::stdout, *f); !n) {
            eprintln("cat: {}", n.error().message());
            status = 1;
        }
    }
    return status;
}
```

## See also

- [flags](flags/README.md): the command line read into variables, as Go's `flag` package reads it
- [executable](executable.md): the path of the running program
- [env](env.md), [getenv](getenv.md): the other half of a program's configuration
