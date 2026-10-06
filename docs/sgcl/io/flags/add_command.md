[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::add_command

```cpp
void add_command(const string& name, flags& command);
```

Adds a subcommand: `prog [its flags] name [the command's flags] [its arguments]`, what `git commit` and `go build`
are. The first argument after this description's flags that is `name` hands the rest of the command line to
`command`, a description of its own — its flags, its [positional](positional.md) list, its own commands — and
[chosen](chosen.md) tells the program it was. The command is held by its address, as the variables are: it outlives
the parse. Its description is its line under `Commands:` in the [usage](usage.md), and its own usage is headed
`Usage of <program> name:`. Go's standard library has no subcommands (its programs parse `flag.Args()` again by
hand).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the command's name on the command line: not empty, not beginning with `-` |
| `command` | its description; it outlives the parse |

## Return value

None.

## Complexity

Linear in the number of commands added so far: the name is checked against them.

## Exceptions

`std::invalid_argument` when the name is empty or begins with `-`, and when a command of the name was added already.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    bool verbose = false;
    int port = 8080;
    io::flags root("a tool");
    root.add("v", verbose, "verbose");
    io::flags serve("runs the server", {{"port", port, "the port"}});
    io::flags check("checks the configuration");
    root.add_command("serve", serve);
    root.add_command("check", check);
    auto r = root.parse(vector<string>{"-v", "serve", "-port", "9090"});
    println("{} {} {} {}", bool(r), verbose, serve.chosen(), port);
    println("{}", root.parse(vector<string>{"stop"}).error().path());
    vector<string> lines(root.usage().split('\n'));
    for (const string& line : lines.as_slice(1)) {
        if (!line.empty()) {
            println("{}", line);
        }
    }
}
```

Output:

```text
true true true 9090
unknown command: stop
a tool
  -v	verbose
Commands:
  check
    	checks the configuration
  serve
    	runs the server
```

## See also

- [chosen](chosen.md): which command the parse chose
- [parse](parse.md): the command line read
- [sgcl::io::flags](README.md)
