[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::code

```cpp
error_code code() const noexcept;
```

Returns the code of the failure: `errno` in the system category for a failed system call, or an [errc](../errc.md)
in the category of the module ([category](../category.md)). An `error_code` compares by category as well as value:
`code() == std::errc::no_such_file_or_directory`, a comparison with the condition, matches whatever category the
platform reports it in, where a comparison with `std::make_error_code(...)`, of the generic category, misses a code
of the system one; `code() == io::errc::closed` compares with a code of the module directly. The predicates of the
class ask the common questions so.

## Parameters

None.

## Return value

The `error_code`; a value-initialized one (0 in the system category) for a default-constructed error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    error_code code = io::open("missing.txt").error().code();
    println("{} {}", code == std::errc::no_such_file_or_directory, code.category().name());
    println("{}", code == std::make_error_code(std::errc::no_such_file_or_directory));

    error_code eof = io::read_full(io::buffer("ab"), vector<byte>(4)).error().code();
    println("{} {}", eof == io::errc::unexpected_eof, eof.category().name());
}
```

Output:

```text
true system
false
true io
```

## See also

- [errc](../errc.md), [category](../category.md): the codes of the module
- [message](message.md): the text of the error
- [sgcl::io::error](../error.md)
