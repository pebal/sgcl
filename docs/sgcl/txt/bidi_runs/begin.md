[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](../bidi_runs.md)

# sgcl::txt::bidi_runs::begin

```cpp
auto begin() const noexcept;
```

Returns an iterator to the leftmost piece, the first to be drawn. The iterator is a random access iterator over the
pieces the range holds; its `*` is a [run](../bidi_runs-run.md).

## Parameters

None.

## Return value

An iterator to the first piece; equal to [end()](end.md) when there is none.

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
    txt::bidi_runs pieces("Nazwa: שלום 123 OK");
    auto it = pieces.begin();
    println("[{}] then [{}]", it->text, (it + 1)->text);
}
```

Output:

```text
[Nazwa: ] then [123]
```

## See also

- [end](end.md): the iterator past the last piece
- [sgcl::txt::bidi_runs](../bidi_runs.md)
