[sgcl](../README.md) › [io](README.md)

# sgcl::io::terminal_size

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct terminal_size {
        uint16_t rows = 24;
        uint16_t columns = 80;
        uint16_t width_pixels = 0;
        uint16_t height_pixels = 0;

        friend bool operator==(const terminal_size&, const terminal_size&) noexcept = default;
    };
}
```

`sgcl::io::terminal_size` is the size of a terminal: the rows and the columns of characters a program lays its
screen out in, and the pixels of the window when the terminal says them. The system's `struct winsize`, the fields
by name; the default is the classic 24 by 80. Compared field by field.

## Member objects

| Field | Description |
|---|---|
| `rows` | the rows of characters; 24 by default |
| `columns` | the columns of characters; 80 by default |
| `width_pixels` | the width in pixels, 0 when unknown |
| `height_pixels` | the height in pixels, 0 when unknown |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::terminal_size classic;
    io::terminal_size wide = {.rows = 50, .columns = 200};
    println("{}x{} {}", classic.columns, classic.rows, classic == wide);
}
```

Output:

```text
80x24 false
```

## See also

- [get_terminal_size](get_terminal_size.md), [set_terminal_size](set_terminal_size.md): of a terminal's descriptor
- [pty](pty/README.md): [size](pty/size.md) and [resize](pty/resize.md) of a pseudo-terminal
