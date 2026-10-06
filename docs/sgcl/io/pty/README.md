[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::pty

```cpp
#include "sgcl/io/pty.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class pty final : public mixin::reader<pty>, public mixin::writer<pty>;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::pty` is a pseudo-terminal: a pair of descriptors that a program on one end takes for a terminal, what
a terminal emulator, an SSH server or `script` gives the shell it runs. The `pty` is the program's end (the master)
as a stream: what is written to it the program on the terminal reads as typed, echoed and edited by the line
discipline, `^C` its `SIGINT`; what that program writes is read from it. The terminal end (the slave) is the one a
child is started on.

[start](start.md) runs a [command](../command/README.md) on the terminal: the streams the command leaves null are
the terminal, the child leads a session of its own, and the terminal is its controlling terminal — its `^C`, its
`SIGWINCH`, its `/dev/tty`. The child is made without `fork`, as every child of the library is: `posix_spawn`
makes the session and opens the terminal in the child by its name, which a session leader without a controlling
terminal acquires by opening it. [resize](resize.md) sets the size, which the program on the terminal gets as its
`SIGWINCH`.

Go has no pseudo-terminal in its standard library; `github.com/creack/pty` is the one Go programs use, and
`pty.Start(cmd)` is [open_pty](../open_pty.md) and [start](start.md) here. Against it: no `fork` (the size and the
session from `posix_spawn`), the end of the child the end of the stream on every system (Linux's `EIO` is a read
of 0), and an `async_` form of the operations that wait, served by the [reactor](../../async/readable.md).

## Rules

- Made by [open_pty](../open_pty.md): a handle of one word, the copies one pseudo-terminal. A `pty` made by its
  default constructor holds none (`!p`); an operation on it is a contract violation.
- The program keeps its copy of the terminal end from [open_pty](../open_pty.md) to [start](start.md), which
  closes it: a read gives 0, the end of the stream, once the child and what it left on the terminal let go of it.
  A second command is started on the terminal opened again by its name, at the size last given.
- [close](close.md) closes both ends; a child still on the terminal gets `SIGHUP`.
- A stream made of a `pty` (`io::reader in = p;`, [reader](../reader/README.md)) holds the pseudo-terminal itself,
  not the handle.
- One thread or task at a time reads, one writes.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](pty.md) | constructs the handle: no pseudo-terminal, or a copy that is the same one |
| `(destructor)` | releases the handle; the descriptors are closed by `close`, or when the collector finds the pseudo-terminal dead |

#### Reading and writing

| Function | Description |
|---|---|
| [read, async_read](read.md) | reads what the program on the terminal wrote |
| [write, async_write](write.md) | writes what the program on the terminal reads as typed |

#### Terminal

| Function | Description |
|---|---|
| [start](start.md) | starts a command on the terminal, its controlling terminal |
| [resize](resize.md) | sets the size of the terminal |
| [size](size.md) | the size of the terminal |
| [close](close.md) | closes both ends |
| [is_closed](is_closed.md) | checks whether the pseudo-terminal was closed |

#### Observers

| Function | Description |
|---|---|
| [name](name.md) | the path of the terminal end |
| [terminal](terminal.md) | the terminal end, until `start` closes it |
| [fd](fd.md) | the master's descriptor, `-1` when closed |
| [operator bool](operator_bool.md) | checks whether the handle holds a pseudo-terminal |

#### From mixin::reader

[mixin::reader](../mixin/reader/README.md): the algorithms of io over this pseudo-terminal, each with its `async_`
form.

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](../mixin/reader/read_all.md) | reads to the end, into a `vector<byte>` |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | reads to the end, into a `string` |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | copies everything to the end into a writer |

#### From mixin::writer

[mixin::writer](../mixin/writer/README.md): the overloads of `write` and `async_write` beside the pseudo-terminal's
own.

| Function | Description |
|---|---|
| [write, async_write](../mixin/writer/write.md) | writes a text (a `string`, a slice of one, a literal, a `std::string_view`) or one byte |
| [copy_from, async_copy_from](../mixin/writer/copy_from.md) | copies everything from a reader to its end into the pseudo-terminal |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same pseudo-terminal |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty({.rows = 30, .columns = 100}).value();
    io::command sh("/bin/sh", "-c", "read name; echo hello, $name; stty size");
    term.start(sh).value();
    term.write("world\n");  // typed on the terminal, which echoes it
    string screen = term.read_all_text().value();
    print("{}", screen.replace("\r\n", "\n"));
    sh.wait().value();
}
```

Output:

```text
world
hello, world
30 100
```

## See also

- [open_pty](../open_pty.md): what makes one
- [command](../command/README.md): the program started on it
- [terminal_size](../terminal_size.md), [get_terminal_size](../get_terminal_size.md): the size of any terminal
- [is_terminal](../is_terminal.md): whether a descriptor is a terminal
- [net::ssh::server_session](../../net/ssh/server_session/README.md): a session's terminal, a shell run on a `pty`
