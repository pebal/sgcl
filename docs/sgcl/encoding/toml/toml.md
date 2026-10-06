[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::toml

```cpp
toml() noexcept;                           // (1)
toml(bool b) noexcept;                     // (2)
template<class I> toml(I v) noexcept;      // (3)
toml(double d) noexcept;                   // (4)
toml(const string& s) noexcept;            // (5)
toml(const char* s) noexcept;              // (6)
toml(const time::datetime& t) noexcept;    // (7)
toml(const time::date& d) noexcept;        // (8)
```

Constructs a value; an array and a table are made by [array](array.md) and [table](table.md), a local time and a local
date-time by [local_time](local_time.md), or read by [parse](parse.md). None is `explicit`, so
`table({{"port", 8080}})` takes the values as they are.

1. An empty table: the empty document.
2. A boolean, `true` or `false`.
3. An integer of any integral type but `bool` and the characters (takes part only for those), in decimal.
4. A float in its shortest digits, `inf`, `-inf`, `nan`; an integral one with `.0`, so it reads back as a float.
5. A string; its characters shared, not copied.
6. A string of the characters up to the null.
7. An offset date-time: the instant as the clock of its zone shows it, with that zone's offset at it (`Z` for
   UTC), the fraction of a second to its last digit that is not 0.
8. A local date.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the boolean |
| `v` | the integer |
| `d` | the float, or the date |
| `s` | the string, UTF-8 |
| `t` | the instant and its zone |

## Complexity

Constant; (3) to (8) linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml v = encoding::toml::table({
        {"flag", true}, {"count", 42}, {"ratio", 1.0}, {"name", "app"},
        {"when", time::datetime::from_unix(1791271800, time::zone::fixed(2 * hour))},
        {"day", time::date(2026, 10, 6)}});
    print(v.to_string());
}
```

Output:

```text
flag = true
count = 42
ratio = 1.0
name = "app"
when = 2026-10-06T09:30:00+02:00
day = 2026-10-06
```

## See also

- [array](array.md), [table](table.md), [local_time](local_time.md)
- [sgcl::encoding::toml](README.md)
