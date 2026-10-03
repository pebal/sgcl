[sgcl](../../README.md) › [core](../README.md) › [number_error](../number_error.md)

# sgcl::number_error::code

```cpp
constexpr std::errc code() const noexcept;
```

Returns the reason as the code `std::from_chars` reports: `std::errc::result_out_of_range` for a number the type
cannot hold, `std::errc::invalid_argument` for every other reason. Where `std::from_chars` takes a number with more
after it as a success and leaves the rest to the caller, [parse](../parse.md) reports `trailing`, which the code
gives as `invalid_argument`.

## Parameters

None.

## Return value

`std::errc::result_out_of_range` or `std::errc::invalid_argument`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", parse<int>("99999999999").error().code() == std::errc::result_out_of_range);
    println("{}", parse<int>("12px").error().code() == std::errc::invalid_argument);
}
```

Output:

```text
true
true
```

## See also

- [why](why.md): the reason, with `trailing` apart
- [message](message.md): the reason as a text
- [sgcl::number_error](../number_error.md)
