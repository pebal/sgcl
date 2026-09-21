# sgcl::io::error, sgcl::io::result

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    enum class errc;                                      // the module's own codes, one std::error_category
    const std::error_category& category() noexcept;
    error_code make_error_code(errc e) noexcept;
    class error;                                          // code, operation, path
    template<class T = void> using result = expected<T, error>;
    error last_error(const string& op, const string& path = {}) noexcept;   // from errno
}
```

What an operation of io reports when it fails, and the shape every operation returns. `error` is a value: the `error_code` (`errno` in the system category, or an `errc` of the module in its own), the operation (`"open"`, `"read"`, `"mkdir"`) and the path or the name of the stream it was on, so that `message()` reads `open log.txt: no such file or directory`, as Go's `*PathError`. The predicates ask the question a caller asks, whatever the category of the code. `result<T>` is [`expected<T, error>`](../core/expected.md): the value, or the error; `result<>` for an operation that returns nothing. The end of a stream is not an error (a read returns 0); only `read_full`, which was promised more, reports `errc::unexpected_eof`.

## Rules

- An `error` holds two [`string`](../core/string.md)s, so it lives where a `tracked_ptr` may: on a stack, in a managed object, in a `result` on either. Copied freely, compared by code.
- Nothing in the module throws; `r.value()` on a failed result throws `bad_expected_access<error>` with the error inside, for the code that wants exceptions.
- An `error_code` (`sgcl::error_code`, the standard's under the library's name) compares by category as well as value: `e.code() == std::errc::no_such_file_or_directory` (the condition) is the portable test, not `== std::make_error_code(...)`; the predicates do that.

## Members

### errc

```cpp
enum class errc { unexpected_eof = 1, closed, invalid_path, invalid_pattern, line_too_long };
```

The failures no `errno` names: the end of a stream where more was required (`read_full`), a stream closed by the program (a read after `close()`), a path `rel` cannot express or a pattern `match` cannot parse, a line past the bound a `buffered_reader` was given. `make_error_code(errc)` puts one in a `error_code`; `std::is_error_code_enum` is specialized, so `code == errc::closed` compares directly.

### error

```cpp
error();
error(error_code code, const string& op, const string& path = {});
error(errc e, const string& op, const string& path = {});
error_code code() const noexcept;
const string& op() const noexcept;
const string& path() const noexcept;
string message() const;                       // "op path: what the code says"
bool is_not_found() const noexcept;           // ENOENT
bool is_exists() const noexcept;              // EEXIST
bool is_permission() const noexcept;          // EACCES, EPERM
bool is_closed() const noexcept;              // errc::closed, EBADF
bool is_eof() const noexcept;                 // errc::unexpected_eof
bool is_interrupted() const noexcept;         // EINTR
bool is_timeout() const noexcept;             // ETIMEDOUT, EAGAIN, EWOULDBLOCK
friend bool operator==(const error&, const error&) noexcept;   // by code
```

```cpp
auto f = io::open("config.toml");
if (!f) {
    if (f.error().is_not_found()) return defaults();
    std::cerr << f.error().message() << '\n';   // open config.toml: permission denied
    return {};
}
```

### result

```cpp
template<class T = void> using result = expected<T, error>;
```

The value or the error; tested with `if (r)`, read with `*r` / `r->`, the error with `r.error()`. Propagation is two lines, as Go's three: `if (!r) return unexpected(r.error());`, or a chain of `and_then`.

```cpp
io::result<string> first_line(const string& path) {
    auto f = io::open(path);
    if (!f) return unexpected(f.error());
    tracked_ptr lines = make_tracked<io::buffered_reader>(*f);
    auto line = lines->read_line();
    if (!line) return unexpected(line.error());
    return line->value_or(string());
}
```

### last_error

```cpp
error last_error(const string& op, const string& path = {}) noexcept;
```

`error(error_code(errno, std::system_category()), op, path)`: what a stream of your own returns after a system call failed.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

int main() {
    auto r = io::read_text("/etc/hosts");
    if (!r) {
        std::cerr << r.error().message() << '\n';
        return r.error().is_permission() ? 2 : 1;
    }
    std::cout << r->size() << " bytes\n";
    auto bad = io::open("/nonexistent/file");
    std::cout << bad.error().message() << ": not found? " << bad.error().is_not_found() << '\n';
}
```

## See also

- [expected](../core/expected.md): the type under `result`; [file](file.md), [fs](fs.md): the operations that return one
- `tests/io/stream.cpp`: `ErrorCarriesOpPathAndCode`.
