[sgcl](../README.md) › [io](README.md)

# sgcl::io::parse_flags

```cpp
#include "sgcl/io/flags.h"   // or "sgcl/io.h"

namespace sgcl::io {
    void parse_flags(int argc, char** argv, std::initializer_list<flag> list, const string& description = {});
}
```

The program's command line read into the flags of `list`, in one call: a [flags](flags/README.md) made of them and
its [parse](flags/parse.md)`(argc, argv)`. `-h` prints the usage on the standard error and ends the process with
status 0; a command line the flags do not take prints the message and the usage there and ends it with status 2;
otherwise it returns, the variables set. The simple case of a program's flags, Go's `flag.Parse()` after its
`flag.IntVar` lines.

## Parameters

| Parameter | Description |
|---|---|
| `argc`, `argv` | the arguments of `main` |
| `list` | the flags ([io::flag](flag.md)): a name, a variable, a line of help, the [options](flag_options.md) |
| `description` | what the program does, for the usage |

## Return value

None: it returns only when the command line was taken.

## Complexity

Linear in the number of arguments times the number of flags, and quadratic in the number of flags (each name checked
against the ones before it).

## Exceptions

`std::invalid_argument` for a name refused or given twice, as [flags::add](flags/add.md) throws it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main(int argc, char** argv) {
    int port = 8080;
    bool verbose = false;
    vector<string> tags;
    io::parse_flags(argc, argv, {{"port", port, "the port", {.short_name = "p", .env = "APP_PORT"}},
                                 {"verbose", verbose, "log every request", {.short_name = "v"}},
                                 {"tag", tags, "a tag, repeated"}});
    println("port {}, verbose {}, {} tags", port, verbose, tags.size());
}
```

Output:

```text
port 8080, verbose false, 0 tags
```

## See also

- [flags](flags/README.md): a description kept, with commands and a positional list
- [flag](flag.md), [flag_options](flag_options.md)
