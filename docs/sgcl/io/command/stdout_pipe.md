[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::stdout_pipe

```cpp
expected<file, error> stdout_pipe() noexcept;
```

Makes a pipe from the child's standard output before the start and returns the program's end, Go's
`Cmd.StdoutPipe`: what the child writes, the program reads there. The pipe becomes `out`. The child's end is
blocking, as a program expects its standard streams; the program's end stays non-blocking, on the
[reactor](../../async/readable.md). [start](start.md) closes the child's end in the program. The program reads the pipe
to its end before [wait](wait.md), which closes it: a wait first may leave the child blocked on a full pipe.

## Parameters

None.

## Return value

The program's end of the pipe, a [file](../file/README.md), or the error of [pipe](../pipe.md).

## Complexity

Constant: one pipe.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command seq("sh", "-c", "echo one; echo two; echo three");
    io::file from = seq.stdout_pipe().value();
    (void)seq.start();
    io::buffered_reader lines(from);
    for (string_slice line : lines.lines()) {
        println("[{}]", line);
    }
    (void)seq.wait();
}
```

Output:

```text
[one]
[two]
[three]
```

## See also

- [stdin_pipe](stdin_pipe.md), [stderr_pipe](stderr_pipe.md): the other two streams
- [output](output.md): the output gathered into a string
- [buffered_reader](../buffered_reader/README.md): the lines of a stream
- [sgcl::io::command](README.md)
