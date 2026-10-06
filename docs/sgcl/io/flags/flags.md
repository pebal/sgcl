[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::flags

```cpp
flags() noexcept = default;                                            // (1)
explicit flags(const string& description) noexcept;                    // (2)
flags(std::initializer_list<flag> list);                               // (3)
flags(const string& description, std::initializer_list<flag> list);    // (4)
```

Makes a description of a command line.

1. With no flags and no text of its own.
2. With `description`, which the [usage](usage.md) prints after its first line.
3. With the flags of `list` added in their order, each an [io::flag](../flag.md): a description in one line,
   `io::flags({{"port", port, "the port"}, {"v", verbose, "verbose"}})`.
4. The same with `description`.

The copy, the move and the assignments are the implicit ones: a copy has the flags added so far, the list of them
shared, and what is added to the copy afterwards is the copy's alone.

## Parameters

| Parameter | Description |
|---|---|
| `description` | what the program does, for the usage |
| `list` | the flags, each a name, a variable, a line of help and its [options](../flag_options.md) |

## Complexity

- (1–2) Constant.
- (3–4) Quadratic in the number of flags: each name is checked against the ones before it.

## Exceptions

- (1–2) None.
- (3–4) What [add](add.md) throws: `std::invalid_argument` for a name refused or given twice.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    io::flags f("serves the files named");
    f.add("port", port, "the `port` to listen on");
    vector<string> lines(f.usage().split('\n'));
    for (const string& line : lines.as_slice(1)) {
        if (!line.empty()) {
            println("{}", line);
        }
    }
}
```

Output:

```text
serves the files named
  -port port
    	the port to listen on (default 8080)
```

A description in one line, parsed:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    bool verbose = false;
    io::flags f("a server", {{"port", port, "the port", {.short_name = "p"}}, {"verbose", verbose, "verbose"}});
    auto r = f.parse(vector<string>{"-p", "9090", "-verbose"});
    println("{} {} {}", bool(r), port, verbose);
}
```

Output:

```text
true 9090 true
```

## See also

- [add](add.md): a flag tied to a variable
- [parse_flags](../parse_flags.md): the program's command line in one call
- [usage](usage.md): the text of the description
- [sgcl::io::flags](README.md)
