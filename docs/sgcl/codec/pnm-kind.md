[sgcl](../README.md) › [codec](README.md) › [pnm](pnm/README.md)

# sgcl::codec::pnm::kind

```cpp
#include "sgcl/codec/pnm.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class pnm {
    public:
        enum class kind : uint8_t { automatic, pbm, pgm, ppm, pam };
    };
}
```

`sgcl::codec::pnm::kind` names the Netpbm format [encode](pnm/encode.md) writes, in its
[options](pnm-options.md). The kind of a file read is not asked: [decode](pnm/decode.md) reads every one.

| Value | Description |
|---|---|
| `automatic` | PGM for a gray image, PAM for one with alpha, PPM for any other; the default |
| `pbm` | black and white, a pixel black where its gray is below half (P4, or P1 plain) |
| `pgm` | gray, 8 or 16 bits (P5, or P2 plain) |
| `ppm` | RGB, 8 or 16 bits (P6, or P3 plain) |
| `pam` | the image's own channels, gray or RGB, with or without alpha (P7, no plain form) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(1, 1, codec::pixel_format::rgb8);
    for (codec::pnm::kind k : {codec::pnm::kind::pbm, codec::pnm::kind::pgm, codec::pnm::kind::ppm,
                               codec::pnm::kind::pam}) {
        vector<byte> file = codec::pnm::encode(picture, {.kind = k});
        println("{}{}: {} bytes", char(file[0]), char(file[1]), file.size());
    }
}
```

Output:

```text
P4: 8 bytes
P5: 12 bytes
P6: 14 bytes
P7: 62 bytes
```

## See also

- [options](pnm-options.md): what holds the kind
- [encode](pnm/encode.md): what writes it
- [sgcl::codec::pnm](pnm/README.md)
