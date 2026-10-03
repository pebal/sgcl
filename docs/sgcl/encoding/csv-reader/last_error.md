[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [reader](README.md)

# sgcl::encoding::csv::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

The mistake the reader stopped at, Go's `ParseError`: empty while the reader reads well and when it has reached the
end, so after a loop of [next()](next.md) it tells the end from a mistake. Its [code](../error/code.md) says what
went wrong, its [line](../error/line.md) and [column](../error/column.md) where, in code points, and its
[message()](../error/message.md) both:

| Code | When | Go |
|---|---|---|
| `syntax` | a quote in a field without quotes; a character after a closing quote | `ErrBareQuote`, `ErrQuote` |
| `unexpected_end` | a quoted field the input ends in | `ErrQuote` |
| `field_count` | a record of another length than the first, with [options](../csv-options.md)`::same_field_count` | `ErrFieldCount`; Go also returns the record |
| `out_of_range` | a record of a stream past `options::max_record_size`; a number past its field's type ([read\<T\>](read.md)) | none |
| `type_mismatch`, `missing_field`, `unsupported_value` | a field of a type ([read\<T\>](read.md)) | none |
| `io` | a read of the stream failed: the stream's error is in [io_error()](../error/io_error.md) | the stream's error |

## Parameters

None.

## Return value

The error, or an empty optional.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* text : {"a,b\nc,d\n", "a,b\nc\n", "a,\"b\n", "a,b\"c\n"}) {
        encoding::csv::reader r(text);
        while (r.next()) {
        }
        if (auto& e = r.last_error()) {
            println("{} at {}:{}", e->message(), e->line(), e->column());
        } else {
            println("the end");
        }
    }
}
```

Output:

```text
the end
2:1: wrong number of fields: 1, the first record has 2 at 2:1
1:6: extraneous or missing " in a quoted field at 1:6
1:4: bare " in a field without quotes at 1:4
```

## See also

- [error](../error/README.md): the code, the place and the message
- [next](next.md): `nullopt` at the end and at a mistake
- [sgcl::encoding::csv::reader](README.md)
