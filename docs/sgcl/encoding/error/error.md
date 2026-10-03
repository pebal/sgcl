[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::error

```cpp
/*(1)*/ error() noexcept = default;
/*(2)*/ error(errc code, uint64_t offset, const string& detail = {}) noexcept;
/*(3)*/ error(const io::error& e, uint64_t offset) noexcept;
```

Constructs an error. The formats of the module make their own; a program makes one for an input it reads itself and
wants reported in the same shape, in the same `expected`.

1. An error of the code `syntax` at offset 0, with no place, path or detail.
2. The code at the offset; `detail`, when given, is what [message()](message.md) says in place of the code's own
   words (`"invalid character '*'"`, `"expected a number, found a string"`). The line and the column are 0 until
   [locate](locate.md) or [set_position](set_position.md) gives them.
3. The source or the sink failed at the offset: the code is `io`, and the stream's error is kept in
   [io_error()](io_error.md).

## Parameters

| Parameter | Description |
|---|---|
| `code` | what went wrong |
| `offset` | the byte of the input, from its start |
| `detail` | the words of the message, in place of the code's |
| `e` | the stream's error |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<int, encoding::error> parse_digit(const string& text) {
    if (text.size() != 1 || text[0] < '0' || text[0] > '9') {
        return unexpected(encoding::error(encoding::errc::syntax, 0, "a digit expected"));
    }
    return text[0] - '0';
}

int main() {
    println("{}", parse_digit("7").value());
    println("{}", parse_digit("x").error().message());
    println("{}", encoding::error(encoding::errc::unexpected_end, 12).message());
    println("{}", encoding::error().message());
}
```

Output:

```text
7
offset 0: a digit expected
offset 12: unexpected end of input
offset 0: syntax error
```

## See also

- [errc](../errc.md): the codes
- [locate](locate.md), [set_position](set_position.md), [set_path](set_path.md): the place, set after
- [sgcl::encoding::error](../error.md)
