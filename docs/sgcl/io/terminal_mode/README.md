[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::terminal_mode

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class terminal_mode;
}
```

`sgcl::io::terminal_mode` is a terminal whose modes [make_raw](../make_raw.md) or
[disable_echo](../disable_echo.md) changed, and the guard that gives them back: [restore](restore.md), or the
destructor when nothing restored them, puts back the modes the terminal had before, so that no return and no exception
leaves a terminal without its echo or its line editing. Go's `term.MakeRaw` and `term.Restore` of `golang.org/x/term`
as one object, the way `std::lock_guard` is a lock and its release.

## Rules

- Made by [make_raw](../make_raw.md) and [disable_echo](../disable_echo.md); one made by its default constructor holds no terminal and restores nothing.
- Moved, not copied: one guard restores. A guard moved from holds nothing; one assigned to restores its own
  terminal first.
- An object of its scope, on the stack: the terminal is raw from [make_raw](../make_raw.md) to the end of the
  scope.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](terminal_mode.md) | constructs the guard: none, or the one moved from |
| `(destructor)` | restores the terminal, unless restored already |
| [operator=](operator_assign.md) | restores its own terminal and takes another's guard |

#### Terminal

| Function | Description |
|---|---|
| [restore](restore.md) | gives the terminal its modes from before |

#### Observers

| Function | Description |
|---|---|
| [fd](fd.md) | the terminal's descriptor, `-1` when none |
| [operator bool](operator_bool.md) | checks whether it holds a terminal to restore |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::file keyboard = term.terminal();
    {
        io::terminal_mode raw = io::make_raw(keyboard.fd()).value();
        term.write("q");  // a key, no Enter after it
        byte key;
        keyboard.read(slice<byte>(&key, 1)).value();
        println("key {}", char(key));
    }  // the terminal as it was
    term.write("a line\n");
    byte line[16];
    size_t n = keyboard.read(line).value();
    println("{} bytes, a line again", n);
}
```

Output:

```text
key q
7 bytes, a line again
```

## See also

- [make_raw](../make_raw.md): what makes one
- [disable_echo](../disable_echo.md): the echo off, by lines
- [crypto::read_password](../../crypto/read_password.md): a password read under it
- [is_terminal](../is_terminal.md)
