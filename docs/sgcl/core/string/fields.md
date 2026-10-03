[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::fields

```cpp
pieces fields() const noexcept;
```

Splits the string at runs of white space: the words between them, in order, as a range of slices into the string,
[pieces](../string-pieces.md), as Go's `strings.Fields`.

The white space is Unicode's, [unicode::is_space](../unicode.md): the space, the tab, the newline and the rest of the
C locale's six, and the no-break, the ideographic and the other spaces. No word is empty: white space at the ends
gives no piece, a run of it is one gap, and a string of white space alone, or the empty string, has no word. In a
`wstring`, a `u16string` and a `u32string` each unit is tested as a code point.

Nothing is searched by the call: each word is found as the walk reaches it, one search per word, and nothing is
allocated. Each word is a [slice](../slice.md) that holds the string's object, valid on its own wherever it is kept.

## Parameters

None.

## Return value

The range of the words, a [pieces](../string-pieces.md) holding this string; its elements are `string_slice`s,
`slice<const CharT>`.

## Complexity

Constant. The walk of the range is linear in the length of the string.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string line = "  alpha\tbeta \u00A0 gamma\u3000";  // a no-break and an ideographic space
    for (string_slice word : line.fields()) {
        print("[{}]", word);
    }
    println("");

    vector<string_slice> words(line.fields());
    println("{} {}", words.size(), words[1].owner().get() == line.object());
    println("{}", string(" \t\n").fields().empty());
}
```

Output:

```text
[alpha][beta][gamma]
3 true
true
```

## See also

- [split](split.md): the pieces between the occurrences of a separator
- [trim](trim.md): without white space at both ends
- [pieces](../string-pieces.md): the range `fields` returns
- [unicode](../unicode.md): what white space is
- [sgcl::string](../string.md)
