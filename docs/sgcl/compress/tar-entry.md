[sgcl](../README.md) › [compress](README.md) › [tar](tar.md)

# sgcl::compress::tar::entry

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    struct entry {
        string name;
        string link_name;
        tar::kind type = tar::kind::file;
        uint64_t size = 0;
        io::permissions mode = io::permissions::none;
        time::datetime modified;
        optional<time::datetime> accessed;
        optional<time::datetime> changed;
        int64_t uid = 0;
        int64_t gid = 0;
        string user_name;
        string group_name;
        uint32_t dev_major = 0;
        uint32_t dev_minor = 0;
        vector<pair<string, string>> pax;

        bool is_local() const noexcept;
        friend bool operator==(const entry& a, const entry& b) = default;
    };
}
```

`sgcl::compress::tar::entry` is one entry of a tar archive, Go's `tar.Header`: what the headers before its data say,
merged into one — the ustar header, the archive's global pax records and the entry's own, GNU's long name and link.
The [reader](tar-reader.md)'s [next](tar-reader/next.md) gives one; the [writer](tar-writer.md)'s
[write_header](tar-writer/write_header.md) takes one and writes ustar when it fits, pax when it does not.

## Rules

- The names are the archive's bytes as they are: nothing is checked or cleaned when an archive is read, and
  [is_local](tar-entry/is_local.md) says whether a name may be joined to a directory.
- `size` is the bytes of data the entry has in the archive, which the reader gives and the writer takes: 0 for
  everything but a file (the size a header of a directory or a link may carry is not data, where Go gives it).
- The times are in UTC, to the nanosecond a pax record gives; ustar holds whole seconds from 1970 to 2242.
- An aggregate, a value: `{.name = "a.txt", .size = 5}`, the other fields at their defaults.

## Member objects

| Member | Description |
|---|---|
| `name` | the path in the archive, `/` between the parts; a directory's ends in `/` |
| `link_name` | the target of a symlink or a hard link |
| `type` | what the entry is ([kind](tar-kind.md)); `kind::file` by default |
| `size` | the bytes of data: 0 for everything but a file |
| `mode` | the permission bits |
| `modified` | the time of the last change of the data, in UTC |
| `accessed`, `changed` | the times of the last access and of the last change of the inode, when the archive has them (pax, GNU, star) |
| `uid`, `gid` | the owner's and the group's ids |
| `user_name`, `group_name` | the owner's and the group's names |
| `dev_major`, `dev_minor` | the device numbers of a `char_device` or a `block_device` |
| `pax` | the pax records none of the fields above holds, in order (`"comment"`, `"SCHILY.xattr.user.a"`) |

## Member functions

| Function | Description |
|---|---|
| [is_local](tar-entry/is_local.md) | checks whether the name, and a link's target, stay inside the directory |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](tar-entry/operator_cmp.md) | every field equal |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    compress::tar::entry e{.name = "notes.txt", .size = 6, .mode = io::permissions(0640)};
    e.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    e.user_name = "ann";
    (void)w.write_header(e);
    (void)w.write("notes\n");
    (void)w.close();

    compress::tar::reader r(archive);
    auto read = r.next();
    println("{} {} {} {}", (*read)->name, (*read)->size, (*read)->modified, (*read)->user_name);
    println("{}", **read == e);
}
```

Output:

```text
notes.txt 6 2026-09-21T14:13:20Z ann
true
```

## See also

- [reader](tar-reader.md), [writer](tar-writer.md)
- [sgcl::compress::tar](tar.md)
