[sgcl](../../README.md) › [txt](../README.md) › [folded_text](../folded_text.md)

# sgcl::txt::folded_text::find

```cpp
optional<occurrence> find(const searcher_type& pattern, size_t from = 0) const noexcept;    // (1)
optional<occurrence> find(const string& pattern, size_t from = 0) const noexcept;           // (2)
```

Finds the first occurrence of a pattern in the mapped text at or after the byte `from` — a byte of the text as it
was given, not of the mapped copy — and answers as the searcher's own [find](../fold_searcher/find.md) does: where
the occurrence begins and the bytes of the text it covers. Only the search is paid: the text was mapped when the
object was built.

1. A pattern mapped once, a [fold_searcher](../fold_searcher.md) for a `folded_text` and a
   [normalized_searcher](../fold_searcher.md) for a `normalized_text`.
2. A pattern as text, mapped on the call.

A match takes whole characters and whole combining sequences, as [find_fold](../find_fold.md) says. An empty
pattern is found at `from` while `from` is not past the end of the text.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |
| `from` | the byte of the text the search starts at |

## Return value

The [occurrence](../occurrence.md): the byte position in the text where the first occurrence begins and the bytes it
covers, which need not be as many as the pattern has; `nullopt` when there is none. An empty pattern covers no
bytes.

## Complexity

A bisection to `from`, then a scan linear in the rest of the text on ordinary text and the text times the pattern
at worst; (2) maps the pattern first, linear in its length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::folded_text text("Kot, KOT, kot");
    txt::fold_searcher kot("kot");
    for (auto o = text.find(kot); o; o = text.find(kot, o->pos + o->size)) {
        println("{} {}", o->pos, o->size);
    }
    println("{}", txt::folded_text("Die Straße").find("STRASSE")->size);  // seven letters, seven bytes
}
```

Output:

```text
0 3
5 3
10 3
7
```

## See also

- [contains](contains.md): whether there is an occurrence
- [occurrence](../occurrence.md): what it answers
- [count](count.md): the number of occurrences
- [sgcl::txt::folded_text, normalized_text](../folded_text.md)
