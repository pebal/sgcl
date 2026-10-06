[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::apply_patch

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

expected<string, patch_error> apply_patch(const string& text, const string& patch,
                                          const patch_options& o = {});
```

Applies a unified patch to a text as `patch` does: the hunks of the patch's first file (after its `---` and `+++`
lines, or from its first hunk when it has none; what follows a next `---` or `diff` line is another file's), in
order. A hunk goes where its line numbers say, moved by what the hunks before it moved; else to the nearest place
after the hunk before where all its old lines stand, looking down and up in turn; else, with fuzz, with the first
and the last lines of its context not compared — one line at each end, then two, up to `o.fuzz` and no more than the
context it has at either end. The lines of context written are the text's, so a fuzzed line keeps the text's
version. A hunk with context at its end only (and one run of changes) was made at the start of the text and goes
only there, one with context at its start only goes only at the end; a hunk whose new lines end without a line feed
ends the text.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text the patch was made from (or, reversed, to) |
| `patch` | the patch: `diff -u`, `git diff` or [unified_diff](unified_diff.md) output |
| `o` | the [patch_options](patch_options.md): the fuzz, reversal |

## Return value

The patched text, or a [patch_error](patch_error/README.md): a hunk header that is not one, a hunk shorter than its
header says, a line without a mark, a patch of no hunk, or a hunk that stands nowhere in the text.

## Complexity

Linear in the sizes of the text and the patch when every hunk stands where its numbers say; a hunk searched for
costs its lines times the lines it is searched across.

## Exceptions

None.

## Notes

The oracle of the tests is the `patch` of macOS: the anchoring of hunks at the ends of the text and the bounds of
the fuzz are its, and the two agree on 99 random cases in a hundred. Where they differ, `patch` joins a line to the
next where a hunk adds lines after a last line without a line feed, or refuses a reversed hunk that ends without
one; this function does neither.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string patch = "--- a\n+++ b\n@@ -1,3 +1,3 @@\n one\n-two\n+2\n three\n";
    print("{}", txt::apply_patch("zero\none\ntwo\nthree\n", patch).value());
    print("{}", txt::apply_patch("one\n2\nthree\n", patch, {.reverse = true}).value());
    auto r = txt::apply_patch("one\nthree\n", patch);
    println("hunk {}, line {}: {}", r.error().hunk(), r.error().line(), r.error().message());
}
```

Output:

```text
zero
one
2
three
one
two
three
hunk 1, line 3: a hunk whose lines are not in the text
```

## See also

- [unified_diff](unified_diff.md)
- [patch_options](patch_options.md)
- [patch_error](patch_error/README.md)
- [sgcl::txt](README.md)
