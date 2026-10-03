[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::number_text

```cpp
optional<string> number_text() const noexcept;
```

The literal of a number kept as its text, as the input wrote it: an integer past what an `uint64_t` holds, which
is never rounded, and with [options](../json-options.md)`::keep_number_text` any number that is not an integer
literal (`0.1`, `1e2`), for amounts that cannot pass through a double. It is Go's `json.Number` under `UseNumber`;
an integer literal that an `int64_t` or an `uint64_t` holds is held as one and has no text, integers being never
rounded anyway.

## Parameters

None.

## Return value

The literal, or `nullopt` for any other value: a number held as an integer or a double, and every value that is
not a number.

## Complexity

Constant.

## Exceptions

None.

## Notes

A number kept as its text is still a number: it is written back as its literal, [as_int](as_int.md),
[as_uint](as_uint.md) and [as_double](as_double.md) read it, and two of them compare by their digits.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = R"([12, 123456789012345678901234567890, 0.10, 1e2])";
    encoding::json plain = encoding::json::parse(text);
    for (auto& v : plain.elements()) {
        println("{} {}", v.to_string(), v.number_text());
    }

    encoding::json::options keep;
    keep.keep_number_text = true;
    encoding::json exact = encoding::json::parse(text, keep);
    for (auto& v : exact.elements()) {
        println("{} {}", v.to_string(), v.number_text());
    }
    println(exact[2] == encoding::json::parse("0.1", keep).value());
}
```

Output:

```text
12 nullopt
123456789012345678901234567890 "123456789012345678901234567890"
0.1 nullopt
100 nullopt
12 nullopt
123456789012345678901234567890 "123456789012345678901234567890"
0.10 "0.10"
1e2 "1e2"
true
```

## See also

- [options](../json-options.md): `keep_number_text`
- [as_double](as_double.md): the number, rounded
- [sgcl::encoding::json](../json.md)
