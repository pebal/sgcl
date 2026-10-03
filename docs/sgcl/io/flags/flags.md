[sgcl](../../README.md) › [io](../README.md) › [flags](../flags.md)

# sgcl::io::flags::flags

```cpp
/*(1)*/ flags() noexcept = default;
/*(2)*/ explicit flags(const string& description) noexcept;
```

Makes a description of a command line with no flags.

1. Without a text of its own.
2. With `description`, which the [usage](usage.md) prints after its first line.

The copy, the move and the assignments are the implicit ones: a copy has the flags added so far, the list of them
shared, and what is added to the copy afterwards is the copy's alone.

## Parameters

| Parameter | Description |
|---|---|
| `description` | what the program does, for the usage |

## Complexity

Constant.

## Exceptions

None.

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

## See also

- [add](add.md): a flag tied to a variable
- [usage](usage.md): the text of the description
- [sgcl::io::flags](../flags.md)
