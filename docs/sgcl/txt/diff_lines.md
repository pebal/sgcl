[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_lines

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

vector<diff_edit> diff_lines(const string& a, const string& b, const diff_options& o = {});
```

Returns the edit script that turns `a` into `b` by lines: each line with its line feed (a last line without one is
another line than the same text with it), compared whole. Myers' algorithm finds a shortest script — the fewest
lines removed and inserted; patience diff anchors on the lines that occur once in each text first, and reads as a
person would where code moved. Of the scripts of the same length, the one where every run of changes stands as low
as it can is returned, as diff and git print it: an inserted line among equal ones is the last of them.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the old text |
| `b` | the new text |
| `o` | the [diff_options](diff_options.md): the algorithm, whether white space counts |

## Return value

The edit script: runs of lines equal, removed from `a` and inserted from `b`, in the order of the texts, as byte
ranges of both; empty when both texts are empty.

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

// each line of the edits, marked as diff marks it
void show(const string& a, const string& b, const vector<txt::diff_edit>& edits) {
    for (const auto& e : edits) {
        bool inserted = e.kind == txt::diff_kind::insert;
        const string& text = inserted ? b : a;
        size_t at = inserted ? e.new_begin : e.old_begin, end = inserted ? e.new_end : e.old_end;
        char mark = inserted ? '+' : e.kind == txt::diff_kind::remove ? '-' : ' ';
        while (at < end) {
            size_t next = text.find('\n', at) + 1;
            print("{} {}", mark, text.substr(at, next - at));
            at = next;
        }
    }
}

int main() {
    string a = "one\ntwo\nthree\n", b = "one\n2\nthree\nfour\n";
    show(a, b, txt::diff_lines(a, b));
}
```

Output:

```text
  one
- two
+ 2
  three
+ four
```

## See also

- [diff_words](diff_words.md)
- [diff_chars](diff_chars.md)
- [unified_diff](unified_diff.md)
- [diff_edit](diff_edit.md)
- [sgcl::txt](README.md)
