[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::merge3

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

merge_result merge3(const string& base, const string& ours, const string& theirs,
                    const merge_options& o = {});
```

Merges two texts made from one, by lines, as `git merge-file` and `diff3 -m` do: a change made on one side only is
taken, the same change made on both is taken once, and changes of the same lines — or of lines that touch — are a
conflict, written between markers: `<<<<<<<` with the label of ours, our lines, `=======`, their lines, `>>>>>>>`
with the label of theirs (and, with `diff3`, the base's lines after `|||||||`). Without `diff3` a conflict is
narrowed to the lines that differ, as git narrows it; a side that ends without a line feed gets one before a marker.

## Parameters

| Parameter | Description |
|---|---|
| `base` | the common ancestor |
| `ours` | one side |
| `theirs` | the other side |
| `o` | the [merge_options](merge_options.md): the labels, the base in conflicts |

## Return value

The [merge_result](merge_result.md): the merged text and the number of conflicts in it.

## Complexity

As two [diff_lines](diff_lines.md) of the base and the sides, and linear in the size of the texts.

## Exceptions

None.

## Notes

Where several shortest alignments of a side against the base exist, the regions of a conflict can differ from git's,
which picks among them by heuristics of its own; on random merges the two agree about 97 times in a hundred, and the
merges where they differ are equally short.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string base = "a\nb\nc\nd\n";
    auto clean = txt::merge3(base, "A\nb\nc\nd\n", "a\nb\nc\nD\n");
    print("{} conflicts\n{}", clean.conflicts, clean.text);
    auto clash = txt::merge3(base, "a\nmine\nc\nd\n", "a\nyours\nc\nd\n", {.diff3 = true});
    print("{} conflict\n{}", clash.conflicts, clash.text);
}
```

Output:

```text
0 conflicts
A
b
c
D
1 conflict
a
<<<<<<< ours
mine
||||||| base
b
=======
yours
>>>>>>> theirs
c
d
```

## See also

- [merge_options](merge_options.md)
- [merge_result](merge_result.md)
- [diff_lines](diff_lines.md)
- [sgcl::txt](README.md)
