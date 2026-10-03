[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::error

```cpp
error() noexcept = default;                                                                      // (1)
error(error_code code, const string& op, const string& path = {}, size_t count = 0) noexcept;    // (2)
error(errc e, const string& op, const string& path = {}, size_t count = 0) noexcept;             // (3)
```

Constructs an error. A stream or a function of the program's makes one where it fails, to return in an
`expected<T, io::error>` as the operations of io do.

1. An error with no code (a value-initialized `error_code`, 0 in the system category), no operation, no path and a
   count of 0.
2. An error of `code`, any `error_code`: `errno` in the system category, a code of `std::errc`
   (`std::make_error_code(std::errc::invalid_argument)`), an `errc` of the module.
3. An error of a code of the module, `make_error_code(e)` ([make_error_code](../make_error_code.md)).

[last_error](../last_error.md) makes one from `errno` after a system call failed.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code of the failure |
| `e` | the code of the failure, of the module's own |
| `op` | the operation that failed: `"open"`, `"read"`, a name of the program's |
| `path` | the path or the name of the stream it was on; none by default |
| `count` | what the operation had done when it failed, the bytes of a short read ([count](count.md)); 0 by default |

## Complexity

Constant: the strings are copied as words.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

expected<int, io::error> port_of(const string& name) {
    if (name == "http") {
        return 80;
    }
    return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "port", name));
}

int main() {
    println("{}", *port_of("http"));
    println("{}", port_of("gopher").error().message());
    io::error cut(io::errc::unexpected_eof, "read", "header");
    println("{}", cut.message());
}
```

Output:

```text
80
port gopher: Invalid argument
read header: unexpected end of stream
```

## See also

- [last_error](../last_error.md): an error from `errno`
- [errc](../errc.md): the module's own codes
- [count](count.md): the bytes done before the failure
- [sgcl::io::error](README.md)
