[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::stdin_pipe

```cpp
expected<file, error> stdin_pipe() noexcept;
```

Makes a pipe to the child's standard input before the start and returns the program's end, Go's `Cmd.StdinPipe`:
what the program writes there, the child reads. The pipe becomes `in`. The child's end is blocking, as a program
expects its standard streams; the program's end stays non-blocking, on the [reactor](../../async/readable.md).
[start](start.md) closes the child's end in the program; [wait](wait.md) closes the program's end once the child
ended, unless the program closed it before, which is how the child sees the end of its input.

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
    io::command grep("grep", "error");
    io::file lines = grep.stdin_pipe().value();  // the program writes what the child reads
    io::file found = grep.stdout_pipe().value();  // and reads what it writes
    (void)grep.start();
    (void)lines.write("no\nan error\nfine\n");
    (void)lines.close();  // the end of the child's input
    auto hits = found.read_all_text();  // read before the wait
    (void)grep.wait();
    print("{}", hits.value_or(""));
}
```

Output:

```text
an error
```

## See also

- [stdout_pipe](stdout_pipe.md), [stderr_pipe](stderr_pipe.md): the pipes the child writes to
- [pipe](../pipe.md): a pipe of the program's
- [sgcl::io::command](README.md)
