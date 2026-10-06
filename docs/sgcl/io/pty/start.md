[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::start

```cpp
expected<void, error> start(command& c) const;
```

Starts `c` on the terminal, as [command::start](../command/README.md) starts a child, with three differences: the
standard streams `c` leaves null (`in`, `out`, `err`) are the terminal; the child leads a session of its own
(`setsid`); and the terminal is its controlling terminal, so that `^C` written to the pseudo-terminal is its
`SIGINT`, [resize](resize.md) its `SIGWINCH`, and `/dev/tty` opens in it. A stream `c` sets stays its own (an
`out` captured into a [buffer](../buffer/README.md) while `in` is the terminal).

On Linux the child is made with `posix_spawn`, as every child of the library: the session is the spawn's
(`POSIX_SPAWN_SETSID`) and the terminal is opened in the child by its [name](name.md), which a session leader
without a controlling terminal acquires by opening it. macOS runs a spawn's file actions before it makes the
session, and no action makes a controlling terminal, so there the child is made as Go makes every child: `fork`,
and in the child nothing but system calls up to the `exec` (the session, the terminal opened and made the
controlling one, the streams, the directory), no lock and no allocation, so that the threads the child does not
have never matter. Either way the session gets every signal at its default and none blocked, as a login session
does: a program started in the background by a shell ignores `SIGINT`, and its child on the terminal does not. Once the child is started the program's copy of the terminal
end is closed, so that a [read](read.md) gives 0 when the child and whatever it left on the terminal let go of it;
a second `start` opens the terminal again by its name and gives it the size last set. The child is waited for
through `c` ([command](../command/README.md)'s `wait` or `async_wait`).

## Parameters

| Parameter | Description |
|---|---|
| `c` | the command to start; its null streams become the terminal |

## Return value

Nothing, or the [error](../error/README.md): `errc::closed` when the pseudo-terminal was closed; the error of the
terminal's open for a second start; the errors of [command::start](../command/README.md) (`errc::not_found` for a
program not in `PATH`, `errc::process_done` for a command started already). A command that did not start is left
as it was given: the streams `start` set are null again.

## Complexity

The spawn of a process.

## Exceptions

What [command::start](../command/README.md) throws.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::buffer out;
    io::command sh("/bin/sh", "-c", "test -t 0 && echo stdin is a terminal; test -t 1 || echo stdout is not");
    sh.out = out;  // set: stays the command's own
    term.start(sh).value();
    term.read_all_text().value();  // until the child let go of the terminal
    sh.wait().value();
    print("{}", out.text());

    io::command missing("no-such-program-sgcl");
    println("{}", term.start(missing).error().message());
}
```

Output:

```text
stdin is a terminal
stdout is not
look_path no-such-program-sgcl: executable file not found in PATH
```

## See also

- [command](../command/README.md): the program, its arguments and its streams
- [resize](resize.md): the size the program sees
- [sgcl::io::pty](README.md)
