[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::error

```cpp
/*(1)*/ const E& error() const& noexcept;
/*(2)*/ E& error() & noexcept;
/*(3)*/ const E&& error() const&& noexcept;
/*(4)*/ E&& error() && noexcept;
```

The error. Precondition: there is no value. `std::expected` leaves `error()` on a value undefined; here the access is
checked, and since the function is `noexcept`, `error()` on an `expected` that holds a value ends the program
(`std::terminate`).

- (1–2) A reference to the error.
- (3–4) An rvalue reference to the error, from an rvalue `expected`.

## Parameters

None.

## Return value

A reference to the error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<int, string> parse_digit(char c) {
    if (c < '0' || c > '9') {
        return unexpected(string(1, c) + " is not a digit");
    }
    return c - '0';
}

int main() {
    for (char c : {'7', 'x'}) {
        auto d = parse_digit(c);
        if (d) {
            println("{}", *d);
        } else {
            println("{}", d.error());
        }
    }
}
```

Output:

```text
7
x is not a digit
```

## See also

- [error_or](error_or.md): the error, or another one when there is none
- [operator bool, has_value](operator_bool.md): checks whether there is a value
- [sgcl::expected\<T, E\>](../expected.md)
