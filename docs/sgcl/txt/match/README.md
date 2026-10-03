[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::match

```cpp
#include "sgcl/txt/regex.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class match;
}
```

`sgcl::txt::match` is one match of a [regex](../regex/README.md): the whole of it and each group, as
[slices](../../core/slice/README.md) of the text it was found in, with their byte positions. It is what
[regex::find](../regex/find.md) returns and what [regex::all](../regex/all.md) walks: Python's `re.Match`, C++'s
`std::match_results`.

A slice holds the object its characters live in, so a match outlives the string it came from and a loop over the
matches of a temporary is safe — which a `std::smatch` over a `std::string_view` is not. A group that took no part
in the match is nothing, which an empty group is not: in `(a)|(b)` over `"b"` group one took no part, and in
`(a?)b` over `"b"` group one matched and is empty. That is why [group](group.md) is an `optional`;
[operator[]](operator_at.md) is the short way for whoever does not care.

## Rules

- A match holds a slice of its text and, where the pattern has groups or names, a `tracked_ptr` to the compiled
  pattern; so it lives where those may, on a stack or inside a managed object. It needs neither the
  [regex](../regex/README.md) nor the string it came from to stay alive.
- The positions are byte offsets in the text: [begin_at](begin_at.md), [end_at](end_at.md). Up to four
  groups their positions are in the match itself, past that in a block of plain memory a copy duplicates.
- A match does not change after it is made; a copy is independent.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](match.md) | constructs an empty match, of no text |
| `(destructor)` | drops the slice of the text |

#### Element access

| Function | Description |
|---|---|
| [text](text.md) | the whole match, as a slice of the text |
| [group](group.md) | a group by number or by name, or nothing when it took no part |
| [operator[]](operator_at.md) | a group by number, empty when it took no part |
| [subject](subject.md) | the whole text the match was found in |

#### Observers

| Function | Description |
|---|---|
| [begin_at](begin_at.md) | the byte position of the start of the match |
| [end_at](end_at.md) | the byte position past the end of the match |
| [empty](empty.md) | checks whether the match has no width |
| [group_count](group_count.md) | the number of groups of the pattern |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    optional<txt::match> m;
    {
        string line = "temp=21.5C";
        m = txt::regex("(?<v>\\d+)\\.(\\d+)(F)?").find(line);
    }  // the string is gone, the match holds its text
    println("{} [{}..{}) in {}", m->text(), m->begin_at(), m->end_at(), m->subject());
    println("{} {} {}", *m->group("v"), (*m)[2], m->group(3).has_value());
}
```

Output:

```text
21.5 [5..9) in temp=21.5C
21 5 false
```

## See also

- [regex](../regex/README.md): the pattern
- [regex_matches](../regex_matches/README.md): every match of a text, as a range
- [slice](../../core/slice/README.md): what a match's pieces are
