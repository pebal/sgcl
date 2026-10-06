[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::content_line

```cpp
content_line() noexcept;                                                                            // (1)
content_line(const string& name, const string& value) noexcept;                                     // (2)
content_line(const string& name, const vector<parameter>& params, const string& value) noexcept;    // (3)
```

1. No name, no value: a line [to_string](to_string.md) refuses until a name is given.
2. A line of the name and the value as a file writes it: escapes are the program's ([text](text.md) makes a TEXT
   line of any characters).
3. The same with parameters, their names upper-cased.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name, in any case |
| `params` | the parameters |
| `value` | the value, as written |

## Complexity

Linear in the size of the line.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line start("dtstart", {{"TZID", {"Europe/Warsaw"}}}, "20261006T090000");
    print(start.to_string());
    println(start.as_datetime()->to_string());
}
```

Output:

```text
DTSTART;TZID=Europe/Warsaw:20261006T090000
2026-10-06T09:00:00+02:00
```

## See also

- [text](text.md)
- [parse](parse.md)
- [sgcl::encoding::content_line](README.md)
