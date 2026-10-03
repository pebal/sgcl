[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](../bidi_runs/README.md) › [run](README.md)

# sgcl::txt::bidi_runs::run::right_to_left

```cpp
bool right_to_left() const noexcept;
```

Checks whether the piece runs right to left: whether its level is odd. A renderer draws the characters of such a
piece in reverse.

## Parameters

None.

## Return value

`true` when `level` is odd.

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
    for (auto piece : txt::bidi_runs("Nazwa: שלום 123 OK")) {
        println("{:>8} {}", piece.text, piece.right_to_left() ? "<-" : "->");
    }
}
```

Output:

```text
 Nazwa:  ->
     123 ->
   שלום  <-
      OK ->
```

## See also

- [sgcl::txt::bidi_runs::run](README.md)
