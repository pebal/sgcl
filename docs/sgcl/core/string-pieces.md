[sgcl](../README.md) › [core](README.md) › [string](string.md)

# sgcl::string::pieces

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string {
    public:
        class pieces;
    };
}
```

`sgcl::string::pieces` is what [split](string/split.md) and [fields](string/fields.md) return: a forward range of
[slices](slice.md) into a string, the pieces between the occurrences of a separator, the words between runs of white
space, or the characters one by one. Each piece is found as the walk reaches it, one search per step, and nothing is
allocated, as with `std::views::split` and Go's `strings.SplitSeq`.

A piece is a `slice<const CharT>`, a `string_slice` of a `string`, that holds the string's object, so it is valid on
its own wherever it is kept: a range-for walks the pieces, a container of slices keeps them (every sequence of the
library has a constructor from a range), a container of strings makes a string of each, and
[join](string/join.md) takes the range as it is.

The range is a value of a few words: the string, the separator and the limit of `split`. The separator is a copy, so
a temporary separator in the head of a range-for cannot dangle: one of up to 16 bytes inside the value, a longer one
in a string of its own, and a string given as the separator held as it is.

## Rules

- A `pieces` holds strings, so it lives where a `tracked_ptr` may: on a stack or inside a managed object
  ([The rules](README.md#the-rules), 1).
- It is made only by `split` and `fields`; it is copied as a value, and a copy walks the same pieces.
- The range is walked again from the start by every `begin()`, and the pieces are computed anew each time: a walk
  costs one search per piece, never an allocation.
- An iterator refers to its `pieces` and is valid while the range lives: a range-for over `s.split(',')` keeps the
  temporary range for the loop, an iterator kept from `s.split(',').begin()` dangles. The slices it hands out hold
  the string and stay valid on their own.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const CharT>` |
| `size_type` | `size_t` |
| `iterator` | a forward iterator of a class of the library, whose `*` is the current piece, `const value_type&` |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | a copy of another `pieces`; a new one is made by `split` and `fields` |
| `(destructor)` | drops the strings it holds |

#### Iterators

| Function | Description |
|---|---|
| [begin](string-pieces/begin.md) | an iterator to the first piece |
| [end](string-pieces/end.md) | the iterator past the last piece |

#### Capacity

| Function | Description |
|---|---|
| [empty](string-pieces/empty.md) | checks whether there is no piece |

#### Observers

| Function | Description |
|---|---|
| [text](string-pieces/text.md) | the string the pieces are of |

## Complexity

A step of the iterator is one search for the separator, or for the next run of white space, from the end of the
previous piece: the whole walk is linear in the length of the string.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string csv = "alice,30,,paris";
    string::pieces cells = csv.split(',');

    for (string_slice cell : cells) {
        print("[{}]", cell);
    }
    println("");

    vector<string_slice> kept(cells);  // slices: each holds the string's object
    vector<string> copied(cells);      // a string made of each piece
    println("{} {} {}", kept.size(), copied[3], string::join(cells, "; "));
}
```

Output:

```text
[alice][30][][paris]
4 paris alice; 30; ; paris
```

## See also

- [split](string/split.md), [fields](string/fields.md): what returns a `pieces`
- [join](string/join.md): one string of a range of parts
- [slice](slice.md): what a piece is
- [sgcl::string](string.md)
