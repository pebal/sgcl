[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::unified_diff

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

string unified_diff(const string& a, const string& b, const unified_options& o = {});
```

Returns the differences of `a` and `b` in the unified format of `diff -u` and `git diff`: the lines `---` and `+++`
with the names, then hunks — `@@ -l,c +l,c @@`, the lines kept with a space in front, the removed with `-`, the
inserted with `+`, and `\ No newline at end of file` under a last line without its line feed. Changes closer than
twice the context share a hunk. What `patch`, `git apply` and [apply_patch](apply_patch.md) read.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the old text |
| `b` | the new text |
| `o` | the [unified_options](unified_options.md): the context, the names, the algorithm, white space |

## Return value

The patch; empty when the texts are equal (or, ignoring white space, equal but for it).

## Complexity

As [diff_lines](diff_lines.md), and linear in the size of the patch.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string before = "a\nb\nc\nd\ne\n", after = "a\nB\nc\nd\ne\nf\n";
    print("{}", txt::unified_diff(before, after,
                                  {.context = 1, .old_name = "a/f.txt", .new_name = "b/f.txt"}));
}
```

Output:

```text
--- a/f.txt
+++ b/f.txt
@@ -1,3 +1,3 @@
 a
-b
+B
 c
@@ -5 +5,2 @@
 e
+f
```

## See also

- [apply_patch](apply_patch.md)
- [unified_options](unified_options.md)
- [diff_lines](diff_lines.md)
- [sgcl::txt](README.md)
