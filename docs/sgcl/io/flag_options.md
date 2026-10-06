[sgcl](../README.md) › [io](README.md)

# sgcl::io::flag_options

```cpp
#include "sgcl/io/flags.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct flag_options {
        string short_name;
        string env;
        bool required = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::flag_options` is what a flag of [io::flags](flags/README.md) may have beyond its name, its variable and
its help: what Go programs write by hand around the `flag` package. Given to [add](flags/add.md) and to an
[io::flag](flag.md), by field name: `{.short_name = "p", .env = "PORT"}`.

## Member objects

| Field | Description |
|---|---|
| `short_name` | a second name of the flag, a letter by convention: `-p 80` beside `-port 80`, and a letter of a combined `-vx` for a bool; empty for none |
| `env` | a variable of the environment whose value is the flag's default when it is set and not empty, read at the parse before the command line, which wins; a list's value split at commas; empty for none |
| `required` | the command line is refused (`flag is required: -port`) when neither it nor `env` gave the flag; `false` by default |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string user;
    int port = 8080;
    io::flags f;
    f.add("user", user, "the user", {.short_name = "u", .required = true});
    f.add("port", port, "the port", {.env = "SGCL_EXAMPLE_PORT"});
    println("{}", f.parse(vector<string>{"-port", "1"}).error().path());
    println("{}", bool(f.parse(vector<string>{"-u", "ann"})));
}
```

Output:

```text
flag is required: -user
true
```

## See also

- [flags::add](flags/add.md): a flag with its options
- [flag](flag.md): a flag as a value
