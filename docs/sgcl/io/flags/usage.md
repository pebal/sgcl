[sgcl](../../README.md) › [io](../README.md) › [flags](../flags.md)

# sgcl::io::flags::usage

```cpp
string usage() const noexcept;
```

Returns the usage that `-h` prints, Go's `flag.Usage` with `flag.PrintDefaults` in it: `Usage of <program>:` with
the program's `argv[0]` from [args](../args.md), the description on the next line, then the flags sorted by name,
each `  -name type` with its help after a tab on the same line for a bool of one letter and on the next line
otherwise, after four spaces and a tab. A word of the help in backquotes is the type's name (`the \`port\` to listen
on` gives `-port port`, the help without the quotes), and the default follows the help unless it is the type's zero
value, a string's quoted: ` (default 8080)`, ` (default "info")`. The positional list comes last, as `name...`.

## Parameters

None.

## Return value

The text of the usage, a line after each entry.

## Complexity

Linear in the number of flags, with a sort of their names.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    string level = "info";
    bool verbose = false;
    io::flags f("serves the files named");
    f.add("port", port, "the `port` to listen on");
    f.add("level", level, "the log level");
    f.add("v", verbose, "log every request");
    print("{}", f.usage());
}
```

Sample output:

```text
Usage of ./serve:
serves the files named
  -level string
    	the log level (default "info")
  -port port
    	the port to listen on (default 8080)
  -v	log every request
```

## See also

- [parse](parse.md): prints the usage for `-h` and after a refused command line
- [(constructor)](flags.md): the description
- [sgcl::io::flags](../flags.md)
