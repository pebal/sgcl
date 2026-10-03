[sgcl](../../README.md) › [txt](../README.md) › [match](README.md)

# sgcl::txt::match::end_at

```cpp
size_t end_at() const noexcept;
```

Returns the byte position in the text just past the match: Python's `m.end()`. The match is the bytes from
[begin_at](begin_at.md) up to here, and a search for the next match starts here, which is what
[regex::find](../regex/find.md) takes as `from`.

## Parameters

None.

## Return value

The position one past the last byte of the match; `begin_at()` for a match of no width.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex number("\\d+");
    string text = "12 kg, 3 szt.";
    for (auto m = number.find(text); m; m = number.find(text, m->end_at())) {
        print("{}..{} ", m->begin_at(), m->end_at());
    }
    println();
}
```

Output:

```text
0..2 7..8 
```

## See also

- [begin_at](begin_at.md): the position of the start
- [sgcl::txt::match](README.md)
