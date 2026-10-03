[sgcl](../README.md) › [io](README.md)

# sgcl::io::errc

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class errc {
        unexpected_eof = 1,
        closed,
        invalid_path,
        invalid_pattern,
        line_too_long,
        not_found,
        exit_status,
        process_done,
        wait_delay,
        unsupported,
        insecure_path,
        invalid_argument,
        help_requested
    };
}

template<>
struct std::is_error_code_enum<sgcl::io::errc> : std::true_type {};
```

The failures of io that no `errno` names. They form the category of the module, `"io"` ([category](category.md)),
beside the system one: every error of the module is a `std::error_code` of either. `std::is_error_code_enum` is
specialized, so an `errc` converts to an `error_code` ([make_error_code](make_error_code.md)) and
`code == io::errc::closed` compares directly. The text of each is what [message](error/message.md) prints after the
operation and the path.

| Value | Description |
|---|---|
| `unexpected_eof` | "unexpected end of stream": the stream ended where more was required, a [read_full](read_full.md) with the buffer part filled (Go's `io.ErrUnexpectedEOF`) |
| `closed` | "stream closed": a stream closed by the program, an operation after `close()` or one waiting when the close came |
| `invalid_path` | "invalid path": a path [rel](path/rel.md) cannot express |
| `invalid_pattern` | "invalid pattern": a pattern [match](path/match.md) or [glob](path/glob.md) cannot parse |
| `line_too_long` | "line too long": a line past the bound a [buffered_reader](buffered_reader/README.md) was given |
| `not_found` | "executable file not found in PATH": no executable of the name in `PATH`, from [look_path](look_path.md) |
| `exit_status` | "the process ended with a failure status": the wait of a child process; the status in the [command](command/README.md)'s state |
| `process_done` | "process already finished": a second wait, or a signal after the wait or the release of a [process](process/README.md) |
| `wait_delay` | "wait delay expired": the copying tasks of a command outlasted its `wait_delay` after the child ended, and the pipes were closed |
| `unsupported` | "descriptor number past the reactor's table": a wait on a descriptor whose number is past the reactor's table, four million numbers; an operation that would wait fails rather than try again forever |
| `insecure_path` | "insecure path": a name that would leave its directory once joined to it, from [path::under](path/under.md) (Go's `ErrInsecurePath`) |
| `invalid_argument` | "invalid command line": a command line the [flags](flags/README.md) do not take, Go's message about it in the error's path |
| `help_requested` | "help requested": `-h` or `-help` on the command line of the [flags](flags/README.md) (Go's `flag.ErrHelp`); its message is `flag: help requested` |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto r = io::read_full(io::buffer("abc"), vector<byte>(8));
    println("{} {}", r.error().code() == io::errc::unexpected_eof, r.error().is_eof());

    error_code code = io::errc::line_too_long;
    println("{}: {}", code.category().name(), code.message());
}
```

Output:

```text
true true
io: line too long
```

## See also

- [error](error/README.md): the code, the operation, the path
- [category](category.md), [make_error_code](make_error_code.md)
