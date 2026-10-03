[sgcl](../../README.md) › [io](../README.md) › [command](../command.md)

# sgcl::io::command::stderr_pipe

```cpp
expected<file, error> stderr_pipe() noexcept;
```

Makes a pipe from the child's standard error before the start and returns the program's end, Go's
`Cmd.StderrPipe`: what the child writes to its standard error, the program reads there. The pipe becomes `err`. As
with [stdout_pipe](stdout_pipe.md), the child's end is blocking, [start](start.md) closes it in the program, and the
program reads the pipe to its end before [wait](wait.md), which closes it.

## Parameters

None.

## Return value

The program's end of the pipe, a [file](../file.md), or the error of [pipe](../pipe.md).

## Complexity

Constant: one pipe.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command check("sh", "-c", "echo 'disk almost full' >&2; exit 1");
    io::file errors = check.stderr_pipe().value();
    (void)check.start();
    auto text = errors.read_all_text();
    auto ended = check.wait();
    println("{}: {}", ended.has_value(), text.value_or("").trim());
}
```

Output:

```text
false: disk almost full
```

## See also

- [stdout_pipe](stdout_pipe.md), [stdin_pipe](stdin_pipe.md): the other two streams
- [output](output.md): the standard error in `captured_err` of a failure
- [sgcl::io::command](../command.md)
