[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::error

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class error;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::error` is what an operation of io reports when it fails: the `error_code` (`errno` in the system category,
or an [errc](../errc.md) of the module in its own, [category](../category.md)), the operation (`"open"`, `"read"`,
`"mkdir"`) and the path or the name of the stream it was on, so that [message](message.md) reads `open
log.txt: No such file or directory`, as Go's `*PathError`. The predicates (`is_not_found`, `is_permission`, ...)
ask the question a caller asks, whatever the category of the code. It is a value: copied, compared by code, held in
an `expected`.

Every operation of the module returns an [expected](../../core/expected/README.md)`<T, error>`: the value, or the error;
`expected<void, error>` for an operation that returns nothing. There is no alias: it is the same `expected<T, E>` as
the whole library's, whose `E` is the error of the call. It is tested with `if (r)`, read with `*r` and `r->`, its
error with `r.error()`, and passed on in two lines, as Go's three: `if (!r) return unexpected(r.error());`, or a
chain of `and_then`. The end of a stream is not an error (a read returns 0); only a read that was promised more,
[read_full](../read_full.md) ending part way, reports `errc::unexpected_eof`, with the bytes it got as the error's
[count](count.md).

What differs from `std`: `std::filesystem` throws a `filesystem_error` with the code and the paths, or fills an
`error_code` given by reference, which carries no path; the error here is the code, the operation and the path in
one value, returned. A missing file, a reset connection or a full disk is an outcome the code handles where it
occurs, which a return value states and an exception hides; and in a server a `throw` per dropped connection, a
microsecond and a lock on the unwinder each, would be the most expensive path of the program.

## Rules

- An error is copied freely.
- Nothing in the module throws its errors: `r.value()` on a failed result throws
  [bad_expected_access](../../core/bad_expected_access/README.md)`<error>` with the error inside, whose `what()` is the
  error's message, for the code that wants exceptions.
- An `error_code` (`sgcl::error_code`, the standard's under the library's name) compares by category as well as
  value: `e.code() == std::errc::no_such_file_or_directory`, a comparison with the condition, is the portable test,
  where `== std::make_error_code(...)` compares with the generic category and misses a code of the system one. The
  predicates do that.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error.md) | constructs an error of a code, an operation, a path and a count |
| `(destructor)` | drops the two strings |
| `operator=` | copies or moves another error |

#### Observers

| Function | Description |
|---|---|
| [code](code.md) | the `error_code`: `errno` in the system category, or an `errc` |
| [op](op.md) | the operation that failed |
| [path](path.md) | the path or the stream it was on |
| [count](count.md) | the bytes done before the failure: what `read_full` read before the end |
| [message](message.md) | the text: the operation, the path and what the code says |

#### Predicates

| Function | Description |
|---|---|
| [is_not_found](is_not_found.md) | checks whether nothing is at the path (`ENOENT`, `errc::not_found`) |
| [is_exists](is_exists.md) | checks whether something is at the path already (`EEXIST`) |
| [is_permission](is_permission.md) | checks whether the operation was not permitted (`EACCES`, `EPERM`) |
| [is_closed](is_closed.md) | checks whether the stream was closed (`errc::closed`, `EBADF`) |
| [is_eof](is_eof.md) | checks whether the stream ended before what was required (`errc::unexpected_eof`) |
| [is_interrupted](is_interrupted.md) | checks whether a signal interrupted the call (`EINTR`) |
| [is_timeout](is_timeout.md) | checks whether the operation ran out of time (`ETIMEDOUT`, `EAGAIN`, `EWOULDBLOCK`) |
| [is_exit_status](is_exit_status.md) | checks whether a child process ended with a failure status (`errc::exit_status`) |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two errors by their codes |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

expected<string, io::error> first_line(const string& path) {
    auto opened = io::open(path);
    if (!opened) {
        return unexpected(opened.error());
    }
    io::buffered_reader lines(opened);
    auto line = lines.read_line();
    if (!line) {
        return unexpected(line.error());
    }
    return *line ? string(**line) : string();  // a copy: the line is the reader's memory
}

int main() {
    io::write_file("notes.txt", "first\nsecond\n");
    println("{}", *first_line("notes.txt"));

    auto missing = first_line("missing.txt");
    if (!missing) {
        println("{}; not found: {}", missing.error().message(), missing.error().is_not_found());
    }
}
```

Output:

```text
first
open missing.txt: No such file or directory; not found: true
```

## See also

- [errc](../errc.md): the module's own codes
- [last_error](../last_error.md): an error from `errno`
- [expected](../../core/expected/README.md), [bad_expected_access](../../core/bad_expected_access/README.md): the result, the exception
  of `value()`
- [file](../file/README.md), [stat](../stat.md): operations that return one
