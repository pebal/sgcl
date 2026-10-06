[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::load, load_all, async_load

```cpp
static expected<vcard, error> load(const string& path);                         // (1)
static expected<vector<vcard>, error> load_all(const string& path);             // (2)
static async::task<expected<vcard, error>> async_load(string path) noexcept;    // (3)
```

The cards of a file, read with the default options:

1. Its one card.
2. Every card of it: an address book.
3. (1) for a task, the file read on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The card or the cards, or the [error](../error/README.md): [parse](parse.md)'s, with its line and column, for the
text; `io` without a place, `io_error()` saying why, for a file that does not open or read.

## Complexity

Linear in the length of the file.

## Exceptions

- (1–2) What a read of the file throws.
- (3) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("contacts.vcf", "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Ann\r\nEND:VCARD\r\nBEGIN:VCARD\r\nVERSION:4.0\r\nFN:Bob\r\nEND:VCARD\r\n");
    println(encoding::vcard::load_all("contacts.vcf")->size());
    println(encoding::vcard::load("contacts.vcf").error().message());
}
```

Output:

```text
2
1:1: more than one VCARD
```

## See also

- [save](save.md)
- [sgcl::encoding::vcard](README.md)
