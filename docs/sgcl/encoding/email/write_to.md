[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::write_to

```cpp
expected<void, io::error> write_to(const io::writer& w) const;                            // (1)
expected<void, io::error> write_to(const io::writer& w, const write_options& o) const;    // (2)
```

The text of [to_string](to_string.md) written to `w`.

## Parameters

| Parameter | Description |
|---|---|
| `w` | where it goes: a file, a buffer, a connection |
| `o` | how the message is written |

## Return value

Nothing, or the `io::error` of the writer.

## Complexity

Linear in the size of the message.

## Exceptions

What the writer's write throws; the writers of the library throw nothing.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "Hi", "body");
    io::buffer out;
    m.write_to(out);
    println("{}", out.text().view().ends_with("\r\n\r\nbody"));
}
```

Output:

```text
true
```

## See also

- [to_string](to_string.md)
- [email](README.md)
