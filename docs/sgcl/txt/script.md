[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::script

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class script : uint16_t {
        unknown, adlam, ahom, /* ... one for each script of Unicode 16 ... */ yi, zanabazar_square
    };
}
```

The script a code point is written in, as [script_of](script_of.md) answers it: one enumerator for each script of
Unicode 16, generated with the tables, in the order of their names in lower case, and the three that are not writing
systems — `common`, `inherited` and `unknown`, which is zero. A name is Unicode's in lower case: `Old_Permic` is
`old_permic`, `Nko` is `nko`.

| Value | Description |
|---|---|
| `unknown` | no script: a code point no script claims; zero |
| `adlam` | Unicode's `Adlam` |
| `ahom` | Unicode's `Ahom` |
| `anatolian_hieroglyphs` | Unicode's `Anatolian_Hieroglyphs` |
| `arabic` | Unicode's `Arabic` |
| `armenian` | Unicode's `Armenian` |
| `avestan` | Unicode's `Avestan` |
| `balinese` | Unicode's `Balinese` |
| `bamum` | Unicode's `Bamum` |
| `bassa_vah` | Unicode's `Bassa_Vah` |
| `batak` | Unicode's `Batak` |
| `bengali` | Unicode's `Bengali` |
| `bhaiksuki` | Unicode's `Bhaiksuki` |
| `bopomofo` | Unicode's `Bopomofo` |
| `brahmi` | Unicode's `Brahmi` |
| `braille` | Unicode's `Braille` |
| `buginese` | Unicode's `Buginese` |
| `buhid` | Unicode's `Buhid` |
| `canadian_aboriginal` | Unicode's `Canadian_Aboriginal` |
| `carian` | Unicode's `Carian` |
| `caucasian_albanian` | Unicode's `Caucasian_Albanian` |
| `chakma` | Unicode's `Chakma` |
| `cham` | Unicode's `Cham` |
| `cherokee` | Unicode's `Cherokee` |
| `chorasmian` | Unicode's `Chorasmian` |
| `common` | what many scripts share: the digits, the punctuation, the space (Unicode's `Common`) |
| `coptic` | Unicode's `Coptic` |
| `cuneiform` | Unicode's `Cuneiform` |
| `cypriot` | Unicode's `Cypriot` |
| `cypro_minoan` | Unicode's `Cypro_Minoan` |
| `cyrillic` | Unicode's `Cyrillic` |
| `deseret` | Unicode's `Deseret` |
| `devanagari` | Unicode's `Devanagari` |
| `dives_akuru` | Unicode's `Dives_Akuru` |
| `dogra` | Unicode's `Dogra` |
| `duployan` | Unicode's `Duployan` |
| `egyptian_hieroglyphs` | Unicode's `Egyptian_Hieroglyphs` |
| `elbasan` | Unicode's `Elbasan` |
| `elymaic` | Unicode's `Elymaic` |
| `ethiopic` | Unicode's `Ethiopic` |
| `garay` | Unicode's `Garay` |
| `georgian` | Unicode's `Georgian` |
| `glagolitic` | Unicode's `Glagolitic` |
| `gothic` | Unicode's `Gothic` |
| `grantha` | Unicode's `Grantha` |
| `greek` | Unicode's `Greek` |
| `gujarati` | Unicode's `Gujarati` |
| `gunjala_gondi` | Unicode's `Gunjala_Gondi` |
| `gurmukhi` | Unicode's `Gurmukhi` |
| `gurung_khema` | Unicode's `Gurung_Khema` |
| `han` | Unicode's `Han` |
| `hangul` | Unicode's `Hangul` |
| `hanifi_rohingya` | Unicode's `Hanifi_Rohingya` |
| `hanunoo` | Unicode's `Hanunoo` |
| `hatran` | Unicode's `Hatran` |
| `hebrew` | Unicode's `Hebrew` |
| `hiragana` | Unicode's `Hiragana` |
| `imperial_aramaic` | Unicode's `Imperial_Aramaic` |
| `inherited` | a combining mark, which takes the script of what it sits on (Unicode's `Inherited`) |
| `inscriptional_pahlavi` | Unicode's `Inscriptional_Pahlavi` |
| `inscriptional_parthian` | Unicode's `Inscriptional_Parthian` |
| `javanese` | Unicode's `Javanese` |
| `kaithi` | Unicode's `Kaithi` |
| `kannada` | Unicode's `Kannada` |
| `katakana` | Unicode's `Katakana` |
| `kawi` | Unicode's `Kawi` |
| `kayah_li` | Unicode's `Kayah_Li` |
| `kharoshthi` | Unicode's `Kharoshthi` |
| `khitan_small_script` | Unicode's `Khitan_Small_Script` |
| `khmer` | Unicode's `Khmer` |
| `khojki` | Unicode's `Khojki` |
| `khudawadi` | Unicode's `Khudawadi` |
| `kirat_rai` | Unicode's `Kirat_Rai` |
| `lao` | Unicode's `Lao` |
| `latin` | Unicode's `Latin` |
| `lepcha` | Unicode's `Lepcha` |
| `limbu` | Unicode's `Limbu` |
| `linear_a` | Unicode's `Linear_A` |
| `linear_b` | Unicode's `Linear_B` |
| `lisu` | Unicode's `Lisu` |
| `lycian` | Unicode's `Lycian` |
| `lydian` | Unicode's `Lydian` |
| `mahajani` | Unicode's `Mahajani` |
| `makasar` | Unicode's `Makasar` |
| `malayalam` | Unicode's `Malayalam` |
| `mandaic` | Unicode's `Mandaic` |
| `manichaean` | Unicode's `Manichaean` |
| `marchen` | Unicode's `Marchen` |
| `masaram_gondi` | Unicode's `Masaram_Gondi` |
| `medefaidrin` | Unicode's `Medefaidrin` |
| `meetei_mayek` | Unicode's `Meetei_Mayek` |
| `mende_kikakui` | Unicode's `Mende_Kikakui` |
| `meroitic_cursive` | Unicode's `Meroitic_Cursive` |
| `meroitic_hieroglyphs` | Unicode's `Meroitic_Hieroglyphs` |
| `miao` | Unicode's `Miao` |
| `modi` | Unicode's `Modi` |
| `mongolian` | Unicode's `Mongolian` |
| `mro` | Unicode's `Mro` |
| `multani` | Unicode's `Multani` |
| `myanmar` | Unicode's `Myanmar` |
| `nabataean` | Unicode's `Nabataean` |
| `nag_mundari` | Unicode's `Nag_Mundari` |
| `nandinagari` | Unicode's `Nandinagari` |
| `new_tai_lue` | Unicode's `New_Tai_Lue` |
| `newa` | Unicode's `Newa` |
| `nko` | Unicode's `Nko` |
| `nushu` | Unicode's `Nushu` |
| `nyiakeng_puachue_hmong` | Unicode's `Nyiakeng_Puachue_Hmong` |
| `ogham` | Unicode's `Ogham` |
| `ol_chiki` | Unicode's `Ol_Chiki` |
| `ol_onal` | Unicode's `Ol_Onal` |
| `old_hungarian` | Unicode's `Old_Hungarian` |
| `old_italic` | Unicode's `Old_Italic` |
| `old_north_arabian` | Unicode's `Old_North_Arabian` |
| `old_permic` | Unicode's `Old_Permic` |
| `old_persian` | Unicode's `Old_Persian` |
| `old_sogdian` | Unicode's `Old_Sogdian` |
| `old_south_arabian` | Unicode's `Old_South_Arabian` |
| `old_turkic` | Unicode's `Old_Turkic` |
| `old_uyghur` | Unicode's `Old_Uyghur` |
| `oriya` | Unicode's `Oriya` |
| `osage` | Unicode's `Osage` |
| `osmanya` | Unicode's `Osmanya` |
| `pahawh_hmong` | Unicode's `Pahawh_Hmong` |
| `palmyrene` | Unicode's `Palmyrene` |
| `pau_cin_hau` | Unicode's `Pau_Cin_Hau` |
| `phags_pa` | Unicode's `Phags_Pa` |
| `phoenician` | Unicode's `Phoenician` |
| `psalter_pahlavi` | Unicode's `Psalter_Pahlavi` |
| `rejang` | Unicode's `Rejang` |
| `runic` | Unicode's `Runic` |
| `samaritan` | Unicode's `Samaritan` |
| `saurashtra` | Unicode's `Saurashtra` |
| `sharada` | Unicode's `Sharada` |
| `shavian` | Unicode's `Shavian` |
| `siddham` | Unicode's `Siddham` |
| `signwriting` | Unicode's `SignWriting` |
| `sinhala` | Unicode's `Sinhala` |
| `sogdian` | Unicode's `Sogdian` |
| `sora_sompeng` | Unicode's `Sora_Sompeng` |
| `soyombo` | Unicode's `Soyombo` |
| `sundanese` | Unicode's `Sundanese` |
| `sunuwar` | Unicode's `Sunuwar` |
| `syloti_nagri` | Unicode's `Syloti_Nagri` |
| `syriac` | Unicode's `Syriac` |
| `tagalog` | Unicode's `Tagalog` |
| `tagbanwa` | Unicode's `Tagbanwa` |
| `tai_le` | Unicode's `Tai_Le` |
| `tai_tham` | Unicode's `Tai_Tham` |
| `tai_viet` | Unicode's `Tai_Viet` |
| `takri` | Unicode's `Takri` |
| `tamil` | Unicode's `Tamil` |
| `tangsa` | Unicode's `Tangsa` |
| `tangut` | Unicode's `Tangut` |
| `telugu` | Unicode's `Telugu` |
| `thaana` | Unicode's `Thaana` |
| `thai` | Unicode's `Thai` |
| `tibetan` | Unicode's `Tibetan` |
| `tifinagh` | Unicode's `Tifinagh` |
| `tirhuta` | Unicode's `Tirhuta` |
| `todhri` | Unicode's `Todhri` |
| `toto` | Unicode's `Toto` |
| `tulu_tigalari` | Unicode's `Tulu_Tigalari` |
| `ugaritic` | Unicode's `Ugaritic` |
| `vai` | Unicode's `Vai` |
| `vithkuqi` | Unicode's `Vithkuqi` |
| `wancho` | Unicode's `Wancho` |
| `warang_citi` | Unicode's `Warang_Citi` |
| `yezidi` | Unicode's `Yezidi` |
| `yi` | Unicode's `Yi` |
| `zanabazar_square` | Unicode's `Zanabazar_Square` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'a', U'ж', U'α', U'漢', U'1', char32_t(0x0301)}) {
        auto s = txt::script_of(c);
        println("U+{:04X} latin {} common {} inherited {}", uint32_t(c), s == txt::script::latin,
                s == txt::script::common, s == txt::script::inherited);
    }
}
```

Output:

```text
U+0061 latin true common false inherited false
U+0436 latin false common false inherited false
U+03B1 latin false common false inherited false
U+6F22 latin false common false inherited false
U+0031 latin false common true inherited false
U+0301 latin false common false inherited true
```

## See also

- [script_of](script_of.md)
- [is_single_script](is_single_script.md): the scripts of a name
- [sgcl::txt](README.md)
