[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::regex_matches

```cpp
#include "sgcl/txt/regex.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class regex_matches;
}
```

`sgcl::txt::regex_matches` is every match of a [regex](regex.md) in a text, one after another and never
overlapping: what [regex::all](regex/all.md) returns, Python's `re.finditer`. It is a range of the library
([mixin::enumerable](../core/mixin/enumerable.md)), like [words](words.md) and [graphemes](graphemes.md): decided
as it is walked rather than gathered into a container first, so a walk that stops at the first match it wants
searches no further.

The next match is looked for from the end of the last. A match of no width — what `(?:)|x` or `\b` finds — moves
the search on by one code point, or the range would stand still.

## Rules

- A range holds the compiled pattern and a slice of the text, so it lives where a `tracked_ptr` may: on a stack or
  inside a managed object. It needs neither the regex nor the string it came from to stay alive, and a loop over a
  temporary is safe.
- The iterator carries the pattern and the text itself rather than a pointer back to the range: a copy of it
  outlives the range it came from. One machine serves the whole walk, shared by the copies of an iterator, so a
  range is walked by one iterator at a time. A move of an iterator is a copy: the iterator moved from stands where
  it stood and may still be stepped.
- Each walk searches again: [count](regex_matches/count.md) and [empty](regex_matches/empty.md) walk the text, and
  nothing is stored.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one. The [regex](regex.md) it came from is a handle, and a regex moved from is still the pattern.

## Member types

| Type | Definition |
|---|---|
| `value_type` | [match](match.md) |
| `size_type` | `size_t` |
| `iterator` | a forward iterator whose `*` is a `const match&`; two iterators are equal at the same match, or both at the end |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](regex_matches/regex_matches.md) | constructs the range over a text |
| `(destructor)` | drops the pattern and the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](regex_matches/begin.md) | an iterator to the first match |
| [end](regex_matches/end.md) | the iterator past the last match |

#### Capacity

| Function | Description |
|---|---|
| [empty](regex_matches/empty.md) | checks whether there is no match |
| [count](regex_matches/count.md) | the number of matches, walked |

#### Observers

| Function | Description |
|---|---|
| [text](regex_matches/text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the matches, carried by every range of the library
([mixin::enumerable](../core/mixin/enumerable.md)).

| Function | Description |
|---|---|
| `find_if` | the first match the predicate accepts |
| `find_index` | the position, in matches, of the first one the predicate accepts |
| `exists` | checks whether the predicate accepts some match |
| `all` | checks whether the predicate accepts every match |
| `count_of` | the number of matches the predicate accepts |
| `min`, `max` | the first, the last match in the order of a comparison given |
| `for_each` | calls a function with every match |

## Complexity

A step of the iterator is one search from where the last match ended: a whole walk is linear in the length of the
text times the length of the pattern.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex_matches words = txt::regex("\\w+").all(string("Zażółć gęślą jaźń"));
    for (const auto& m : words) {
        print("{}@{} ", m.text(), m.begin_at());
    }
    println();
    println("{} words, a long one: {}", words.count(),
            words.exists([](const txt::match& m) { return m.text().size() > 10; }));
}
```

Output:

```text
Zażółć@0 gęślą@11 jaźń@20 
3 words, a long one: false
```

## See also

- [regex::all](regex/all.md): what makes the range
- [match](match.md): the element
- [mixin::enumerable](../core/mixin/enumerable.md)
