[sgcl](../../README.md) › [io](../README.md) › [flags](../flags.md)

# sgcl::io::flags::parse

```cpp
/*(1)*/ void parse(int argc, char** argv) const;
/*(2)*/ expected<void, error> parse(const vector<string>& args) const;
```

Reads a command line into the variables of the flags, and the arguments after the flags into the
[positional](positional.md) list.

1. The program's command line, as `main` receives it, `argv[0]` its name: Go's `flag.Parse` with
   `flag.ExitOnError`. `-h` prints the usage on the standard error and ends the process with status 0; a command
   line the flags do not take prints Go's message and the usage there and ends the process with status 2, as
   [io::exit](../exit.md) does. Otherwise it returns, the variables set.
2. The arguments given, without the program's name: Go's `FlagSet.Parse` with `ContinueOnError`. Nothing is printed
   and the process runs on; the result says what came of it.

The variables before a refused argument are set, as in Go; the variable of a value refused keeps what it held (Go's
`Set` stores what `strconv` returned, 0 or the type's limit, before it fails).

## Parameters

| Parameter | Description |
|---|---|
| `argc`, `argv` | the arguments of `main` |
| `args` | the arguments after the program's name |

## Return value

- (1) None: it returns only when the command line was taken.
- (2) Nothing, or the [error](../error.md): `errc::help_requested` (Go's `flag.ErrHelp`) for `-h` or `-help`;
  `errc::invalid_argument` for a command line refused, with Go's message as the error's `path()`, so that
  `message()` is `flag <Go's message>: invalid command line`.

## Complexity

Linear in the number of arguments times the number of flags.

## Exceptions

- What `T::parse` of a flag of a type of the program's throws.
- (1) `std::system_error` when the standard error is non-blocking and the thread of the reactor cannot be made for
  the message.

## Example

The program's own command line: run with no arguments, the defaults stay.

```cpp
#include "sgcl/io.h"

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

Run as `serve -port 9090 -v a.txt b.txt`, the same program prints `port 9090, verbose true, 2 files`. Run as
`serve -h`, it prints the usage on the standard error and ends with status 0, as here, where the command line is
made in the program:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    bool verbose = false;
    vector<string> files;
    io::flags f("serves the files named");
    f.add("port", port, "the `port` to listen on");
    f.add("v", verbose, "log every request");
    f.positional("files", files, "the files to serve");
    char name[] = "serve";
    char help[] = "-h";
    char* argv[] = {name, help};
    f.parse(2, argv);
    println("never printed");
}
```

Output:

```text
Usage of serve:
serves the files named
  -port port
    	the port to listen on (default 8080)
  -v	log every request
  files...
    	the files to serve
```

A command line refused, without ending the process:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int port = 8080;
    io::flags f;
    f.add("port", port, "the port");
    for (vector<string> line : {vector<string>{"-port", "x"}, vector<string>{"-port"},
                                vector<string>{"-port=99999999999"}, vector<string>{"---port=1"},
                                vector<string>{"-help"}}) {
        auto r = f.parse(line);
        println("{}", r.error().message());
    }
    println("{}", port);
}
```

Output:

```text
flag invalid value "x" for flag -port: parse error: invalid command line
flag flag needs an argument: -port: invalid command line
flag invalid value "99999999999" for flag -port: value out of range: invalid command line
flag bad flag syntax: ---port=1: invalid command line
flag: help requested
8080
```

## See also

- [usage](usage.md): the text `-h` prints
- [add](add.md), [positional](positional.md): what the parse fills
- [sgcl::io::flags](../flags.md)
