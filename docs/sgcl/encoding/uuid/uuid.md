[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::uuid

```cpp
constexpr uuid() noexcept;                                            // (1)
template<size_t N> explicit consteval uuid(const char (&text)[N]);    // (2)
explicit uuid(const string& text);                                    // (3)
explicit uuid(const array<uint8_t, 16>& bytes) noexcept;              // (4)
```

1. The nil UUID, all zeros.
2. Of a literal in any form [parse](parse.md) takes, in a constant expression: a literal that is not a UUID is an
   error of the compiler. Explicit, as every constructor of a text: `encoding::uuid("…")`.
3. Of a text the program writes: `parse`'s value, or its error thrown.
4. Of its sixteen bytes as they are written, the first the top of `time_low`: a v8 of one's own layout.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the UUID written |
| `bytes` | the sixteen bytes |

## Complexity

Constant.

## Exceptions

- (3) [bad_expected_access](../../core/bad_expected_access/README.md)`<encoding::error>` with `parse`'s error.
- (1), (2), (4) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    constexpr encoding::uuid dns("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    encoding::uuid custom(array<uint8_t, 16>{0x12, 0x34});
    println("{} {}", dns, encoding::uuid().is_nil());
    println(custom);
}
```

Output:

```text
6ba7b810-9dad-11d1-80b4-00c04fd430c8 true
12340000-0000-0000-0000-000000000000
```

## See also

- [parse](parse.md): a text from outside
- [sgcl::encoding::uuid](README.md)
