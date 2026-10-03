[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::is_interrupted

```cpp
bool is_interrupted() const noexcept;
```

Checks whether a signal interrupted the call before it did anything: `EINTR` (`std::errc::interrupted`), whatever
the category it is reported in. A stream of the program's that makes system calls of its own may report it; the
call may be made again.

## Parameters

None.

## Return value

`true` when the code is `EINTR`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int attempts = 0;
    auto flaky = [&]() -> expected<void, io::error> {
        if (++attempts < 3) {
            return unexpected(io::error(std::make_error_code(std::errc::interrupted), "poll"));
        }
        return {};
    };
    auto r = flaky();
    while (!r && r.error().is_interrupted()) {
        r = flaky();
    }
    println("done after {} attempts", attempts);
}
```

Output:

```text
done after 3 attempts
```

## See also

- [last_error](../last_error.md): an error from `errno`
- [sgcl::io::error](../error.md)
