[sgcl](../README.md) › [txt](README.md) › [bidi_runs](bidi_runs.md)

# sgcl::txt::bidi_runs::run

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class bidi_runs {
    public:
        struct run {
            slice<const char> text;
            uint8_t level = 0;
            bool right_to_left() const noexcept;
        };
    };
}
```

`txt::bidi_runs::run` is one piece of [bidi_runs](bidi_runs.md): a run of code points next to each other in the text
and at one level, which a renderer draws as one. The text is in the order it is stored; a piece of odd level has its
characters drawn in reverse.

## Rules

- An aggregate. It holds a slice of the text, so it lives where a `tracked_ptr` may: on a stack or inside a managed
  object ([the rules of core](../core/README.md#the-rules), 1).

## Member objects

| Member | Description |
|---|---|
| `text` | the bytes of the piece, a slice of the text the range was made from |
| `level` | the embedding level the piece runs at: even left to right, odd right to left; 0 by default |

## Member functions

| Function | Description |
|---|---|
| [right_to_left](bidi_runs-run/right_to_left.md) | checks whether the piece runs right to left |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto piece : txt::bidi_runs("abc אבג")) {
        println("[{}] at {}, {} bytes", piece.text, piece.level, piece.text.size());
    }
}
```

Output:

```text
[abc ] at 0, 4 bytes
[אבג] at 1, 6 bytes
```

## See also

- [bidi_runs](bidi_runs.md): the pieces in the order they are drawn
- [mirrored](mirrored.md): the glyphs a right to left piece changes
