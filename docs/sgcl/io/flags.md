# sgcl::io::flags

```cpp
#include "sgcl/io/flags.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class flags;   // a program's flags, each tied to a variable
}
```

The command line of a program, as Go's `flag` package takes it. `add` names the flags, each with the variable it sets and a line of help, and `parse` reads the arguments into the variables. The syntax, the reading of the values, the messages and the usage are Go's, byte for byte: a program ported from Go reads its command line the same way, and prints the same help.

## Rules

- **The syntax** is Go's: `-name` and `--name` are the same flag; a value is `-name=value` or `-name value`, but a bool takes only the first form: `-v` is true, `-v=false` false, and `-v false` is `-v` followed by the argument `false`. `--` ends the flags and is dropped; so does the first argument that is not a flag (`-` alone among them), which is kept. A flag given twice takes the last value.
- **The arguments after the flags** go to the `positional` list, in order, the list cleared first. A description without one refuses them: `unexpected argument: file`, since the program would have no way to see them (Go keeps them in `flag.Args()`).
- **The values** are read as Go's `strconv` reads them: an integer with a base prefix (`0x1F`, `0o17`, `0b101`, `017` octal) and underscores between digits (`1_000`), in the range of the variable's type; a bool as `1`, `t`, `T`, `TRUE`, `true`, `True` or their `0`, `f`, `F`, `FALSE`, `false`, `False`; a floating-point number in decimal or hexadecimal (`0x1p-2`), `inf`, `nan`; a `duration` in Go's text (`1m30s`); a `string` as it is; any other type by its `T::parse(const string&)`.
- **Help**: `-h` and `-help` print the usage, unless the program defined a flag of that name.
- **The usage** is `Usage of <program>:` (argv[0]), the description, then Go's `PrintDefaults`: the flags sorted by name, `  -name type` with the help after a tab on the same line for a bool of one letter and on the next line otherwise, a word of the help in backquotes as the type's name (`the \`port\` to listen on` gives `-port port`), and the default unless it is the type's zero value, a string's quoted; the positional list last, as `name...`.
- **Errors**: `parse(argc, argv)` prints Go's message and the usage on the standard error and ends the process with status 2, or with 0 after the usage asked for, as Go's `flag.Parse`. `parse(args)` prints nothing and ends nothing: `io::errc::help_requested` for `-h`, `io::errc::invalid_argument` for a command line refused, with Go's message as the error's `path()` (`message()` is `flag <Go's message>: invalid command line`). The variables before the refused argument are set, as in Go; the variable of a value refused keeps what it held (Go's `Set` stores what `strconv` returned, 0 or the type's limit, before it fails).
- **The variables** are held by their addresses: they outlive the parse, which is the program's to keep, as in Go. A copy of a `flags` has the flags added so far, so a common set is copied and extended per command.
- **At `add`**: a name that is empty, begins with `-` or holds `=`, and a name added twice, are the program's error: `std::invalid_argument` (Go panics).

## Members

```cpp
flags();
explicit flags(const string& description);

template<class T> void add(const string& name, T& target, const string& help);   // bool, integers, floating point, string, duration, T::parse
void positional(const string& name, vector<string>& target, const string& help);

void parse(int argc, char** argv) const;                       // -h: usage, exit 0; refused: message and usage, exit 2
expected<void, error> parse(const vector<string>& args) const;   // the arguments without the program's name; nothing printed
string usage() const;                                          // what -h prints
```

A common set copied per command, and a command line parsed without ending the process:

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    bool verbose = false;
    int workers = 4;
    io::flags common;
    common.add("v", verbose, "verbose");
    io::flags run = common;                     // common is unchanged
    run.add("workers", workers, "the workers");
    auto r = run.parse(vector<string>{"-v", "-workers=8"});
    println("{} {} {}", bool(r), verbose, workers);
    auto refused = common.parse(vector<string>{"-workers=8"});
    println(refused.error().path());            // Go's message
}
```

Output:

```text
true true 8
flag provided but not defined: -workers
```

## Example

```cpp
#include "sgcl/io/io.h"

using namespace sgcl;

int main(int argc, char** argv) {
    int port = 8080;
    bool verbose = false;
    vector<string> files;
    io::flags f("serves the files named");
    f.add("port", port, "the `port` to listen on");
    f.add("v", verbose, "log every request");
    f.positional("files", files, "the files to serve");
    f.parse(argc, argv);
    println("port {}, verbose {}, {} files", port, verbose, files.size());
}
```

Output:

```text
port 8080, verbose false, 0 files
```

That is the program run with no arguments. Run as `serve -port 9090 -v a.txt b.txt` prints `port 9090, verbose true, 2 files`. Run as `serve -h`, it prints the usage and ends with status 0:

```text
Usage of serve:
serves the files named
  -port port
    	the port to listen on (default 8080)
  -v	log every request
  files...
    	the files to serve
```

Run as `serve -port x`, it prints the reason and the usage and ends with status 2:

```text
invalid value "x" for flag -port: parse error
Usage of serve:
...
```

## SGCL and Go

| Go | SGCL | Note |
|---|---|---|
| `flag.Int("port", 8080, "help")`, `flag.IntVar(&port, ...)` | `f.add("port", port, "help")` | the variable's value is the default |
| `flag.Parse()` | `f.parse(argc, argv)` | the same exit statuses |
| `fs.Parse(args)` with `ContinueOnError` | `f.parse(args)` | nothing printed; `help_requested` for `flag.ErrHelp` |
| `flag.Args()` | `f.positional("files", files, "help")` | left over without it: an error |
| `flag.PrintDefaults()`, `flag.Usage` | `f.usage()` | the description after the first line |
| `flag.Var(value, ...)` | a type with `T::parse(const string&)` and `to_string()` | |

## See also

- [os](os.md): `args()`, `env(name, fallback)`, the other half of a program's configuration; [error](error.md): `io::errc`
- `tests/io/flags.cpp`: Go's flag package as the oracle (`tests/io/go_flags/main.go`: the usage and ninety command lines, the error and every value after each), a common set copied per command, the process ended with 0 and 2; `tests/io/fuzz/flags_fuzz.cpp`: any command line against a model of the parser.
