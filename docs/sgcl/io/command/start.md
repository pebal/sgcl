[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::start

```cpp
expected<void, error> start();
```

Starts the child and returns without waiting for it, Go's `Cmd.Start`. The program is found first: a name without a
`/` is looked up in `PATH` by [look_path](../look_path.md) and `path` becomes what it found; a path is checked to be
an executable file. Then the streams are arranged (the descriptors of the streams that have one, a pipe and a task
for the others, the null device for the empty ones), `dir`, `env` and `set_pgid` applied, and the child made with
`posix_spawn`. The tasks that copy between the streams and the pipes run from then on, as Go's goroutines do, and
when `stop` can be requested a task watches it to kill the child.

`process` holds the child after the call. A command starts once: a second `start()` is an error, and every child is
a command of its own.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md):

- `errc::not_found` when no executable of the name is found (operation `look_path`);
- `errc::process_done` when the command was started already (operation `start`);
- the error of a pipe or of the descriptors arranged for the child;
- the `errno` of `posix_spawn` in the system category (operation `start`, the path the program).

## Complexity

Linear in the number of arguments and of variables of `env`, plus the spawn of a process.

## Exceptions

`std::system_error` when the threads of the scheduler, started by the first task the call spawns (a copying task,
the watcher of `stop`), cannot be made.

## Notes

The child's end of a pipe made by [stdin_pipe](stdin_pipe.md) and the others is closed in the program by the call:
the child has it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command cmd("/bin/sh", "-c", "exit 0");
    if (auto r = cmd.start(); r) {
        println("{} {}", bool(cmd.process), cmd.path);
    }
    (void)cmd.wait();
    println("{}", cmd.start().error().message());

    io::command missing("no-such-program");
    println("{}", missing.start().error().message());
}
```

Output:

```text
true /bin/sh
start /bin/sh: process already finished
look_path no-such-program: executable file not found in PATH
```

## See also

- [wait](wait.md): waits for the child to end
- [run](run.md): the start and the wait in one call
- [sgcl::io::command](README.md)
