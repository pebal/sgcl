[sgcl](../README.md) › [io](README.md)

# sgcl::io::flag

```cpp
#include "sgcl/io/flags.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class flag {
    public:
        template<class T>
        flag(const string& name, T& target, const string& help, const flag_options& options = {}) noexcept;
        template<class T>
        flag(const string& name, vector<T>& target, const string& help, const flag_options& options = {}) noexcept;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::flag` is one flag as a value — its name, the variable it sets, its help, its
[options](flag_options.md) — for the forms that take a list of them in one line: [parse_flags](parse_flags.md) and
the [constructor](flags/flags.md) of [flags](flags/README.md). Written as a braced list, `{"port", port, "the port"}`,
it is what [add](flags/add.md) takes as arguments, for the same types: `bool`, an integer, a floating-point number,
`string`, [duration](../core/duration/README.md), a type with `T::parse(const string&)`, and a `vector` of any of them, a
list. It holds the variable by its address.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    vector<string> tags;
    io::flags f({{"port", port, "the port", {.short_name = "p"}}, {"tag", tags, "a tag"}});
    f.parse(vector<string>{"-p", "1", "-tag", "x"});
    println("{} {}", port, tags.size());
}
```

Output:

```text
1 1
```

## See also

- [parse_flags](parse_flags.md): the program's command line in one call
- [flag_options](flag_options.md)
- [flags](flags/README.md)
