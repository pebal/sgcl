[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::command

```cpp
template<class... Args>
    requires (std::is_convertible_v<const Args&, string> && ...)
explicit command(const string& name, const Args&... arguments) noexcept(/* see below */);    // (1)
command(const string& name, vector<string> arguments) noexcept;                              // (2)
command(command&&) noexcept = default;                                                       // (3)
```

Makes a command of the program `name` and its arguments, Go's `exec.Command`. Nothing runs and nothing is looked up:
`path` is `name` as given until [start](start.md) finds the executable. `argv[0]` of the child is `name` as given,
unless `argv0` says otherwise; the arguments are the rest of its `argv`.

1. The arguments one by one, each anything a [string](../../core/string/README.md) is made from: a string, a literal, a
   slice of text. The constructor is `noexcept` when every argument is text that makes a string without a throw (a
   string, a literal, a C string, a `std::string_view`).
2. The arguments as a vector, taken over: a command line built by the program.
3. Takes another command over, its streams, its process and its tasks. A command is not copied.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the program: a name looked up in `PATH`, or a path when it holds a `/` |
| `arguments` | the arguments after the program's name |

## Complexity

Linear in the number of arguments.

## Exceptions

- (1) None, for arguments of text; what making a string of an argument of another type throws.
- (2–3) None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command status("git", "status", "--short");
    println("{} {}", status.path, status.args);

    vector<string> words = {"-n", "hello"};
    io::command echo("echo", words);
    echo.dir = ".";
    println("{} {} {}", echo.path, echo.args, bool(echo.process));
}
```

Output:

```text
git ["status", "--short"]
echo ["-n", "hello"] false
```

## See also

- [start](start.md), [run](run.md): the child started
- [look_path](../look_path.md): what `path` becomes
- [sgcl::io::command](README.md)
