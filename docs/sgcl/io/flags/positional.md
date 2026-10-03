[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::positional

```cpp
void positional(const string& name, vector<string>& target, const string& help) noexcept;
```

Names the list the arguments after the flags go to, in order, the list cleared first by each parse: Go's
`flag.Args()`, asked for. The usage lists it last, as `name...` with its help. A description without one refuses
such arguments, `unexpected argument: file`, since the program would have no way to see them. A second call
replaces the first.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the list in the usage |
| `target` | the vector the arguments go to; it outlives the parse |
| `help` | the line of help in the usage |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    bool verbose = false;
    vector<string> files;
    io::flags f;
    f.add("v", verbose, "verbose");
    f.positional("files", files, "the files to serve");
    (void)f.parse(vector<string>{"-v", "a.txt", "-b.txt", "c.txt"});
    println("{} {}", verbose, files);
    (void)f.parse(vector<string>{"--", "-v"});
    println("{}", files);

    io::flags strict;
    auto r = strict.parse(vector<string>{"a.txt"});
    println("{}", r.error().path());
}
```

Output:

```text
true ["a.txt", "-b.txt", "c.txt"]
["-v"]
unexpected argument: a.txt
```

## See also

- [add](add.md): a flag tied to a variable
- [parse](parse.md): reads the command line
- [sgcl::io::flags](README.md)
