[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::flags

```cpp
#include "sgcl/io/flags.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class flags;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`io::flags` is the command line of a program, as Go's `flag` package takes it. [add](add.md) names the flags,
each with the variable it sets and a line of help, [positional](positional.md) the list the arguments after
the flags go to, and [parse](parse.md) reads the arguments into the variables. The syntax, the reading of the
values, the messages and the usage are Go's, byte for byte: a program ported from Go reads its command line the same
way and prints the same help.

Beyond Go, each taking effect only where it is used, so that a command line Go takes means the same: a second name of
a flag, the variable of the environment that gives its default and a flag that is required
([flag_options](../flag_options.md)); combined one-letter bools (`-vx`); a list, a `vector` that every occurrence of
the flag appends to; subcommands, each a description of its own ([add_command](add_command.md),
[chosen](chosen.md)); and the one-line forms, a description made of a list of [io::flag](../flag.md) values and
[parse_flags](../parse_flags.md).

What differs from Go: there are no global flags (`flag.Int`), a description is an object of the program's; the
default of a flag is the value of its variable when it is added (Go's `flag.IntVar(&port, "port", 8080, ...)` is
`port = 8080; f.add("port", port, ...)`); the arguments after the flags are an error unless the program asked for
them; and a type of the program's is a flag's type by its `T::parse(const string&)` and `to_string()`, where Go asks
for a `flag.Value`.

## Rules

- **The syntax** is Go's: `-name` and `--name` are the same flag; a value is `-name=value` or `-name value`, but a
  bool takes only the first form: `-v` is true, `-v=false` false, and `-v false` is `-v` followed by the argument
  `false`. `--` ends the flags and is dropped; so does the first argument that is not a flag (`-` alone among them),
  which is kept. A flag given twice takes the last value.
- **The values** are read as Go's `strconv` reads them: an integer with a base prefix (`0x1F`, `0o17`, `0b101`, `017`
  octal) and underscores between digits (`1_000`), in the range of the variable's type; a bool as `1`, `t`, `T`,
  `TRUE`, `true`, `True` or their `0`, `f`, `F`, `FALSE`, `false`, `False`; a floating-point number in decimal or
  hexadecimal (`0x1p-2`), `inf`, `nan`; a [duration](../../core/duration/README.md) in Go's text (`1m30s`); a `string` as it
  is; any other type by its `T::parse(const string&)`.
- **Help**: `-h` and `-help` print the usage, unless the program defined a flag of that name; a command's are the
  command's own.
- **Beyond Go**: a second name is the same flag (`-p` and `-port`); a token that is no flag's name and holds only
  one-letter bools sets each (`-vx`), where Go refuses it; a list's first occurrence in a parse replaces its default;
  the environment is read before the command line, which wins; a required flag is checked after both; the first
  argument after the flags that names a command hands the rest to it.
- **The variables** are held by their addresses: they outlive the parse, which is the program's to keep, as in Go.
- **A copy** of a `flags` has the flags added so far, and a flag added to it afterwards is its own: a common set is
  copied and extended per command. The flags are a list on the managed heap that the copies share, the newest first.
- **Commands** are held by address, as the variables: a command outlives the parse of the description it was added
  to.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](flags.md) | an empty description, with a text for the usage |
| [add](add.md) | a flag tied to a variable |
| [positional](positional.md) | the list of the arguments after the flags |
| [add_command](add_command.md) | a subcommand, a description of its own |
| [chosen](chosen.md) | checks whether the last parse chose this command |
| [parse](parse.md) | reads a command line into the variables |
| [usage](usage.md) | the text `-h` prints |

## Example

A common set, copied and extended per command, and a command line parsed without ending the process.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    bool verbose = false;
    int workers = 4;
    io::flags common;
    common.add("v", verbose, "verbose");
    io::flags run = common;  // common is unchanged by what run adds
    run.add("workers", workers, "the workers");
    auto r = run.parse(vector<string>{"-v", "-workers=8"});
    println("{} {} {}", bool(r), verbose, workers);
    auto refused = common.parse(vector<string>{"-workers=8"});
    println(refused.error().path());  // Go's message
}
```

Output:

```text
true true 8
flag provided but not defined: -workers
```

## See also

- [parse_flags](../parse_flags.md): the program's command line in one call
- [flag](../flag.md), [flag_options](../flag_options.md): a flag as a value, and what it may have beyond Go's
- [args](../args.md): the command line as it is
- [env](../env.md): a variable as a typed value, the other half of a program's configuration
- [error](../error/README.md): `errc::invalid_argument` and `errc::help_requested`
- `tests/io/flags.cpp`: Go's flag package as the oracle (`tests/io/go_flags/main.go`: the usage and ninety command
  lines, the error and every value after each), a common set copied per command, the process ended with 0 and 2;
  `tests/io/flags_beyond_go.cpp`: what is beyond Go; `tests/io/fuzz/flags_fuzz.cpp` and
  `tests/io/fuzz/flags_beyond_fuzz.cpp`: any command line against a model of the parser
