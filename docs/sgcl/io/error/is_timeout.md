[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_timeout

```cpp
bool is_timeout() const noexcept;
```

Checks whether the operation ran out of time: `ETIMEDOUT` (`std::errc::timed_out`), or `EAGAIN` or `EWOULDBLOCK`
(`std::errc::resource_unavailable_try_again`, `std::errc::operation_would_block`), which a read past a socket's
receive timeout reports, whatever the category they are reported in. Go's
`os.IsTimeout`.

## Parameters

None.

## Return value

`true` when the code is one of the three.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::error timed_out(std::make_error_code(std::errc::timed_out), "read", "socket");
    io::error again(std::make_error_code(std::errc::operation_would_block), "read", "socket");
    io::error refused(std::make_error_code(std::errc::connection_refused), "connect", "socket");
    println("{} {} {}", timed_out.is_timeout(), again.is_timeout(), refused.is_timeout());
}
```

Output:

```text
true true false
```

## See also

- [sgcl::io::error](README.md)
