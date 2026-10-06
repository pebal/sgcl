[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::chosen

```cpp
bool chosen() const noexcept;
```

Checks whether the last [parse](parse.md) of the description this one was added to as a command
([add_command](add_command.md)) chose it: the command line named it, its flags were read into its variables. Set
before the command's own flags are read, so that a `-h` of the command, or an error in its flags, leaves it set and
the program prints this command's [usage](usage.md). `false` for a description parsed on its own.

## Parameters

None.

## Return value

`true` when the last parse chose this command.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::flags root;
    io::flags add("adds a file");
    io::flags remove("removes a file");
    root.add_command("add", add);
    root.add_command("rm", remove);
    for (const char* name : {"add", "rm"}) {
        root.parse(vector<string>{name});
        println("{}: add {}, rm {}", name, add.chosen(), remove.chosen());
    }
    auto help = root.parse(vector<string>{"rm", "-h"});
    println("{} {}", help.error().message(), remove.chosen());
}
```

Output:

```text
add: add true, rm false
rm: add false, rm true
flag: help requested true
```

## See also

- [add_command](add_command.md): a command added
- [sgcl::io::flags](README.md)
