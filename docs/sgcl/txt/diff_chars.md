[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_chars

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

vector<diff_edit> diff_chars(const string& a, const string& b, const diff_options& o = {});
```

Returns the edit script that turns `a` into `b` by code points: a code point of UTF-8 is never split (a byte that is
not UTF-8 is a unit of its own).

## Parameters

| Parameter | Description |
|---|---|
| `a` | the old text |
| `b` | the new text |
| `o` | the [diff_options](diff_options.md): the algorithm, whether white space counts |

## Return value

The edit script: runs of code points equal, removed from `a` and inserted from `b`, in the order of the texts, as
byte ranges of both; empty when both texts are empty.

## Complexity

O((N + M) · D) for Myers, N and M the units of the two texts and D the units changed, after the units both share at
the front and the back are set aside; in linear space. Patience adds a sort of the unique units, O(U log U).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

void show(const string& a, const string& b, const vector<txt::diff_edit>& edits) {
    for (const auto& e : edits) {
        if (e.kind == txt::diff_kind::equal) {
            print("[{}]", a.substr(e.old_begin, e.old_end - e.old_begin));
        } else if (e.kind == txt::diff_kind::remove) {
            print("[-{}]", a.substr(e.old_begin, e.old_end - e.old_begin));
        } else {
            print("[+{}]", b.substr(e.new_begin, e.new_end - e.new_begin));
        }
    }
    println("");
}

int main() {
    string a = "kąt", b = "kot";
    show(a, b, txt::diff_chars(a, b));
}
```

Output:

```text
[k][-ą][+o][t]
```

## See also

- [diff_words](diff_words.md)
- [diff_lines](diff_lines.md)
- [diff_edit](diff_edit.md)
- [sgcl::txt](README.md)
