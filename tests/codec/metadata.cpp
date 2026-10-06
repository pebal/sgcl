//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: metadata (metadata.h), the typed reader of EXIF and XMP. EXIF
// blocks made here in both byte orders with every field the reader types,
// read from the block, and through JPEG, PNG, WebP, TIFF and HEIF made
// around them (with XMP in APP1, iTXt plain and compressed, the XMP chunk,
// tag 700, a mime item); ImageIO's reading of the same files (the oracle,
// tools/codec_oracle_metadata.c: what sips and mdls show) field by field,
// and of the system's own pictures carrying EXIF; XMP in its attribute and
// element forms, Seq, Bag and Alt, GPS coordinates, the Flash structure and
// ISO 8601 dates; EXIF's precedence; the boundaries: empty and default
// values, blocks broken at every byte, impossible dates, zero
// denominators, loops of IFD pointers, the limits, streams and load.
#include "oracle.h"

#include "sgcl/compress/zlib.h"
#include "sgcl/time/layout.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::string scratch(const std::string& name) {
        return (scratch_path("sgcl_codec_metadata_tests") / name).string();
    }

    std::string save_bytes(const std::string& name, const std::string& data) {
        const std::string path = scratch(name);
        std::ofstream(path, std::ios::binary).write(data.data(), std::streamsize(data.size()));
        return path;
    }

    std::string text_of(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    slice<const byte> view(const std::string& s) {
        return codec_test::bytes(s);
    }

    // An EXIF block, the TIFF structure: IFD0, the Exif IFD and the GPS
    // IFD, their pointers added, the values past 4 bytes after the IFDs
    class ExifBuilder {
    public:
        explicit ExifBuilder(bool little) : _little(little) {
        }

        enum Ifd { zero, exif, gps };

        void ascii(Ifd i, uint16_t tag, const std::string& s, uint16_t type = 2) {
            std::string v = s;
            v.push_back('\0');
            _add(i, tag, type, uint32_t(v.size()), v);
        }

        void shorts(Ifd i, uint16_t tag, std::initializer_list<uint32_t> v) {
            std::string d;
            for (uint32_t x : v) {
                _put16(d, x);
            }
            _add(i, tag, 3, uint32_t(v.size()), d);
        }

        void longs(Ifd i, uint16_t tag, std::initializer_list<uint32_t> v) {
            std::string d;
            for (uint32_t x : v) {
                _put32(d, x);
            }
            _add(i, tag, 4, uint32_t(v.size()), d);
        }

        void rationals(Ifd i, uint16_t tag, std::initializer_list<std::pair<uint32_t, uint32_t>> v, uint16_t type = 5) {
            std::string d;
            for (auto [n, m] : v) {
                _put32(d, n);
                _put32(d, m);
            }
            _add(i, tag, type, uint32_t(v.size()), d);
        }

        void bytes(Ifd i, uint16_t tag, const std::string& v, uint16_t type = 1) {
            _add(i, tag, type, uint32_t(v.size()), v);
        }

        // A raw entry: the type, count and 4 bytes of value as given
        void raw(Ifd i, uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
            std::string d;
            _put32(d, value);
            _fields[i].push_back({tag, type, count, d, true});
        }

        std::string build(const std::string& tail = {}) const {
            std::vector<Field> f0 = _fields[zero];
            std::string out = _little ? std::string("II\x2a\0", 4) : std::string("MM\0\x2a", 4);
            // the sizes first: IFD0 with its pointers, then the other two
            const bool has_exif = !_fields[exif].empty(), has_gps = !_fields[gps].empty();
            if (has_exif) {
                f0.push_back({0x8769, 4, 1, std::string(4, '\0'), false});
            }
            if (has_gps) {
                f0.push_back({0x8825, 4, 1, std::string(4, '\0'), false});
            }
            std::sort(f0.begin(), f0.end(), [](const Field& a, const Field& b) { return a.tag < b.tag; });
            auto ifd_size = [](size_t n) { return 2 + 12 * n + 4; };
            const size_t at0 = 8, at_exif = at0 + ifd_size(f0.size());
            const size_t at_gps = at_exif + (has_exif ? ifd_size(_fields[exif].size()) : 0);
            size_t data = at_gps + (has_gps ? ifd_size(_fields[gps].size()) : 0);
            for (auto& f : f0) {
                if (f.tag == 0x8769 && !f.literal) {
                    f.data.clear();
                    _put32(f.data, uint32_t(at_exif));
                } else if (f.tag == 0x8825 && !f.literal) {
                    f.data.clear();
                    _put32(f.data, uint32_t(at_gps));
                }
            }
            std::string area;
            _put32(out, uint32_t(at0));
            auto write = [&](std::vector<Field> fields) {
                std::sort(fields.begin(), fields.end(), [](const Field& a, const Field& b) { return a.tag < b.tag; });
                _put16(out, uint32_t(fields.size()));
                for (const auto& f : fields) {
                    _put16(out, f.tag);
                    _put16(out, f.type);
                    _put32(out, f.count);
                    if (f.literal || f.data.size() <= 4) {
                        std::string v = f.data;
                        v.resize(4, '\0');
                        out += v;
                    } else {
                        _put32(out, uint32_t(data + area.size()));
                        area += f.data;
                        if (area.size() & 1) {
                            area.push_back('\0');
                        }
                    }
                }
                _put32(out, 0);
            };
            write(f0);
            if (has_exif) {
                write(_fields[exif]);
            }
            if (has_gps) {
                write(_fields[gps]);
            }
            return out + area + tail;
        }

    private:
        struct Field {
            uint16_t tag;
            uint16_t type;
            uint32_t count;
            std::string data;
            bool literal;
        };

        void _add(Ifd i, uint16_t tag, uint16_t type, uint32_t count, const std::string& d) {
            _fields[i].push_back({tag, type, count, d, false});
        }

        void _put16(std::string& s, uint32_t v) const {
            if (_little) {
                s.push_back(char(v));
                s.push_back(char(v >> 8));
            } else {
                s.push_back(char(v >> 8));
                s.push_back(char(v));
            }
        }

        void _put32(std::string& s, uint32_t v) const {
            if (_little) {
                _put16(s, v & 0xFFFF);
                _put16(s, v >> 16);
            } else {
                _put16(s, v >> 16);
                _put16(s, v & 0xFFFF);
            }
        }

        bool _little;
        std::vector<Field> _fields[3];
    };

    // A camera's block: every field the reader types; the description as
    // EXIF 3.0's UTF-8 (129), or as ASCII holding UTF-8, as older writers
    // put it (ImageIO refuses a TIFF of a type it does not know)
    ExifBuilder camera(bool little, uint16_t text_type = 129) {
        ExifBuilder b(little);
        b.ascii(ExifBuilder::zero, 0x010F, "Canon   ");
        b.ascii(ExifBuilder::zero, 0x0110, "Canon EOS R5");
        b.shorts(ExifBuilder::zero, 0x0112, {6});
        b.ascii(ExifBuilder::zero, 0x0131, "Firmware 1.8.1");
        b.ascii(ExifBuilder::zero, 0x0132, "2024:05:06 18:30:00");
        b.ascii(ExifBuilder::zero, 0x013B, "Jan Kowalski");
        b.ascii(ExifBuilder::zero, 0x8298, "(c) 2024 Jan Kowalski");
        b.ascii(ExifBuilder::zero, 0x010E, "Zachód słońca", text_type);
        b.rationals(ExifBuilder::exif, 0x829A, {{1, 250}});
        b.rationals(ExifBuilder::exif, 0x829D, {{28, 10}});
        b.shorts(ExifBuilder::exif, 0x8827, {400});
        b.ascii(ExifBuilder::exif, 0x9003, "2024:05:06 18:29:41");
        b.ascii(ExifBuilder::exif, 0x9004, "2024:05:06 18:29:41");
        b.ascii(ExifBuilder::exif, 0x9011, "+02:00");
        b.ascii(ExifBuilder::exif, 0x9012, "+02:00");
        b.ascii(ExifBuilder::exif, 0x9291, "25");
        b.rationals(ExifBuilder::exif, 0x9204, {{uint32_t(-2), 3}}, 10);
        b.shorts(ExifBuilder::exif, 0x9209, {0x19});
        b.rationals(ExifBuilder::exif, 0x920A, {{105, 1}});
        b.shorts(ExifBuilder::exif, 0xA405, {105});
        b.longs(ExifBuilder::exif, 0xA002, {8192});
        b.longs(ExifBuilder::exif, 0xA003, {5464});
        b.ascii(ExifBuilder::exif, 0xA433, "Canon");
        b.ascii(ExifBuilder::exif, 0xA434, "RF24-105mm F4 L IS USM");
        b.ascii(ExifBuilder::gps, 0x0001, "S");
        b.rationals(ExifBuilder::gps, 0x0002, {{33, 1}, {51, 1}, {3528, 100}});
        b.ascii(ExifBuilder::gps, 0x0003, "E");
        b.rationals(ExifBuilder::gps, 0x0004, {{151, 1}, {12, 1}, {3036, 100}});
        b.bytes(ExifBuilder::gps, 0x0005, std::string(1, '\1'));
        b.rationals(ExifBuilder::gps, 0x0006, {{125, 10}});
        b.rationals(ExifBuilder::gps, 0x0007, {{16, 1}, {29, 1}, {405, 10}});
        b.ascii(ExifBuilder::gps, 0x001D, "2024:05:06");
        return b;
    }

    std::string fmt(const time::datetime& t) {
        const string s = t.to_string();
        return std::string(s.data(), s.size());
    }

    // What the camera's block reads as
    void expect_camera(const codec::metadata& m) {
        EXPECT_EQ(m.make(), optional<string>("Canon"));
        EXPECT_EQ(m.model(), optional<string>("Canon EOS R5"));
        EXPECT_EQ(m.orientation(), 6u);
        EXPECT_EQ(m.software(), optional<string>("Firmware 1.8.1"));
        EXPECT_EQ(m.artist(), optional<string>("Jan Kowalski"));
        EXPECT_EQ(m.copyright(), optional<string>("(c) 2024 Jan Kowalski"));
        EXPECT_EQ(m.description(), optional<string>("Zachód słońca"));
        ASSERT_TRUE(m.date_time_original());
        EXPECT_EQ(fmt(*m.date_time_original()), "2024-05-06T18:29:41.25+02:00");
        EXPECT_EQ(fmt(*m.date_time_digitized()), "2024-05-06T18:29:41+02:00");
        EXPECT_EQ(fmt(*m.date_time()), "2024-05-06T18:30:00Z");   // no OffsetTime: UTC as asked
        EXPECT_EQ(fmt(*m.date_time(time::zone::fixed(std::chrono::hours(2)))), "2024-05-06T18:30:00+02:00");
        EXPECT_DOUBLE_EQ(*m.exposure_time(), 1.0 / 250);
        EXPECT_DOUBLE_EQ(*m.f_number(), 2.8);
        EXPECT_EQ(m.iso(), optional<uint32_t>(400u));
        EXPECT_DOUBLE_EQ(*m.exposure_bias(), -2.0 / 3);
        EXPECT_EQ(m.flash_fired(), optional<bool>(true));
        EXPECT_DOUBLE_EQ(*m.focal_length(), 105);
        EXPECT_EQ(m.focal_length_35mm(), optional<uint32_t>(105u));
        EXPECT_EQ(m.width(), optional<uint32_t>(8192u));
        EXPECT_EQ(m.height(), optional<uint32_t>(5464u));
        EXPECT_EQ(m.lens_make(), optional<string>("Canon"));
        EXPECT_EQ(m.lens_model(), optional<string>("RF24-105mm F4 L IS USM"));
        auto place = m.location();
        ASSERT_TRUE(place);
        EXPECT_NEAR(place->latitude, -(33 + 51.0 / 60 + 35.28 / 3600), 1e-12);
        EXPECT_NEAR(place->longitude, 151 + 12.0 / 60 + 30.36 / 3600, 1e-12);
        ASSERT_TRUE(place->altitude);
        EXPECT_DOUBLE_EQ(*place->altitude, -12.5);
        EXPECT_EQ(fmt(*m.gps_time()), "2024-05-06T16:29:40.5Z");
    }

    // ImageIO's reading of a file against the module's, every key ImageIO
    // gives
    void expect_as_imageio(const std::string& path, const codec::metadata& m) {
        auto o = run_metadata_oracle(path);
        if (!o) {
            return;
        }
        auto num = [](const std::string& s) { return std::strtod(s.c_str(), nullptr); };
        auto str = [](const optional<string>& s) { return s ? std::string(s->data(), s->size()) : std::string("(none)"); };
        auto near = [](double a, double b, double rel) { return std::fabs(a - b) <= rel * std::max(1.0, std::fabs(b)); };
        const std::pair<const char*, optional<string>> texts[] = {
            {"make", m.make()}, {"model", m.model()}, {"software", m.software()}, {"artist", m.artist()},
            {"copyright", m.copyright()}, {"description", m.description()}, {"lens_make", m.lens_make()}, {"lens_model", m.lens_model()},
        };
        for (const auto& [key, value] : texts) {
            if (o->count(key)) {
                // ImageIO keeps the padding of a text (Make "Canon   "); the
                // module trims it, as exiftool does
                std::string want = o->at(key);
                while (!want.empty() && want.back() == ' ') {
                    want.pop_back();
                }
                EXPECT_EQ(str(value), want) << path << " " << key;
            }
        }
        // a date as ImageIO's text, offset and sub-seconds say it
        auto stamp = [&](const char* key, const char* offset, const char* subsec, const optional<time::datetime>& ours) {
            if (!o->count(key)) {
                return;
            }
            std::string t = o->at(key);
            if (t.size() < 19) {
                return;
            }
            std::string want = t.substr(0, 4) + "-" + t.substr(5, 2) + "-" + t.substr(8, 2) + "T" + t.substr(11, 8);
            if (o->count(subsec)) {
                std::string digits = o->at(subsec);
                while (!digits.empty() && digits.back() == '0') {
                    digits.pop_back();
                }
                if (!digits.empty()) {
                    want += "." + digits;
                }
            }
            ASSERT_TRUE(ours) << path << " " << key;
            if (o->count(offset)) {
                EXPECT_EQ(fmt(*ours), want + o->at(offset)) << path << " " << key;
            } else {
                // ImageIO drops the offset of an XMP date it moves into
                // EXIF's fields; the module keeps it: the clock compared
                EXPECT_EQ(fmt(*ours).substr(0, want.size()), want) << path << " " << key;
            }
        };
        stamp("original", "offset_original", "subsec_original", m.date_time_original());
        stamp("digitized", "offset_digitized", "subsec_digitized", m.date_time_digitized());
        stamp("modified", "offset_modified", "subsec_modified", m.date_time());
        auto real = [&](const char* key, const optional<double>& ours) {
            if (o->count(key)) {
                ASSERT_TRUE(ours) << path << " " << key;
                EXPECT_TRUE(near(*ours, num(o->at(key)), 1e-6)) << path << " " << key << " " << *ours << " " << o->at(key);
            }
        };
        real("exposure", m.exposure_time());
        real("fnumber", m.f_number());
        real("bias", m.exposure_bias());
        real("focal", m.focal_length());
        auto whole = [&](const char* key, const optional<uint32_t>& ours) {
            if (o->count(key)) {
                ASSERT_TRUE(ours) << path << " " << key;
                EXPECT_EQ(double(*ours), num(o->at(key))) << path << " " << key;
            }
        };
        whole("iso", m.iso());
        whole("focal35", m.focal_length_35mm());
        whole("width", m.width());
        whole("height", m.height());
        if (o->count("flash")) {
            EXPECT_EQ(m.flash_fired(), optional<bool>((int(num(o->at("flash"))) & 1) != 0)) << path;
        }
        if (o->count("orientation")) {
            EXPECT_EQ(double(m.orientation()), num(o->at("orientation"))) << path;
        }
        if (o->count("lat") && o->count("lon")) {
            auto place = m.location();
            ASSERT_TRUE(place) << path;
            const double lat = num(o->at("lat")) * (o->count("lat_ref") && o->at("lat_ref") == "S" ? -1 : 1);
            const double lon = num(o->at("lon")) * (o->count("lon_ref") && o->at("lon_ref") == "W" ? -1 : 1);
            // ImageIO rounds to minutes of four decimals: 1e-5 degrees
            EXPECT_NEAR(place->latitude, lat, 1e-5) << path;
            EXPECT_NEAR(place->longitude, lon, 1e-5) << path;
            if (o->count("alt")) {
                ASSERT_TRUE(place->altitude) << path;
                const double alt = num(o->at("alt")) * (o->count("alt_ref") && num(o->at("alt_ref")) == 1 ? -1 : 1);
                EXPECT_NEAR(*place->altitude, alt, 1e-6) << path;
            }
        }
        if (o->count("gps_date") && o->count("gps_time")) {
            auto t = m.gps_time();
            ASSERT_TRUE(t) << path;
            const std::string d = o->at("gps_date"), h = o->at("gps_time");
            const std::string want = d.substr(0, 4) + "-" + d.substr(5, 2) + "-" + d.substr(8, 2) + "T" + h.substr(0, 8);
            EXPECT_EQ(fmt(*t).substr(0, 19), want) << path;
        }
    }

    // A JPEG of one image and the block, an XMP packet in APP1 after it
    std::string jpeg_with(const std::string& exif, const std::string& xmp) {
        codec::image im(16, 16, pixel_format::rgb8);
        if (!exif.empty()) {
            im.set_exif(view(exif));
        }
        std::string file = text_of(*codec::jpeg::encode(im));
        if (!xmp.empty()) {
            std::string body = std::string("http://ns.adobe.com/xap/1.0/") + '\0' + xmp;
            std::string seg = "\xFF\xE1";
            seg.push_back(char((body.size() + 2) >> 8));
            seg.push_back(char(body.size() + 2));
            file.insert(2, seg + body);
        }
        return file;
    }

    uint32_t crc32(const std::string& s) {
        uint32_t c = 0xFFFFFFFFu;
        for (unsigned char b : s) {
            c ^= b;
            for (int k = 0; k < 8; ++k) {
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
            }
        }
        return ~c;
    }

    std::string png_chunk(const std::string& type, const std::string& body) {
        std::string out;
        const uint32_t n = uint32_t(body.size());
        out += char(n >> 24);
        out += char(n >> 16);
        out += char(n >> 8);
        out += char(n);
        const uint32_t c = crc32(type + body);
        out += type + body;
        out += char(c >> 24);
        out += char(c >> 16);
        out += char(c >> 8);
        out += char(c);
        return out;
    }

    // A PNG of one image, the block as eXIf, XMP in an iTXt before IEND
    std::string png_with(const std::string& exif, const std::string& xmp, bool compressed) {
        codec::image im(16, 16, pixel_format::rgb8);
        if (!exif.empty()) {
            im.set_exif(view(exif));
        }
        std::string file = text_of(*codec::png::encode(im));
        if (!xmp.empty()) {
            std::string body = std::string("XML:com.adobe.xmp") + '\0' + char(compressed ? 1 : 0) + '\0' + '\0' + '\0';
            if (compressed) {
                body += text_of(compress::zlib::compress(view(xmp)));
            } else {
                body += xmp;
            }
            file.insert(file.size() - 12, png_chunk("iTXt", body));
        }
        return file;
    }

    // A WebP of one image, the block as EXIF, an XMP chunk, VP8X flags set
    std::string webp_with(const std::string& exif, const std::string& xmp) {
        codec::image im(16, 16, pixel_format::rgb8);
        if (!exif.empty()) {
            im.set_exif(view(exif));
        }
        std::string file = text_of(*codec::webp::encode(im, {.lossless = true}));
        if (!xmp.empty()) {
            if (file.compare(12, 4, "VP8X") != 0) {
                // a simple file: VP8X in front, then the image
                std::string x = "VP8X";
                x += std::string("\x0a\0\0\0", 4);
                std::string flags(10, '\0');
                flags[4] = 15;   // canvas width - 1 = 15, 24 bits
                flags[7] = 15;
                x += flags;
                file.insert(12, x);
            }
            file[20] = char(uint8_t(file[20]) | 0x04);   // the XMP flag
            std::string chunk = "XMP ";
            const uint32_t n = uint32_t(xmp.size());
            chunk += char(n);
            chunk += char(n >> 8);
            chunk += char(n >> 16);
            chunk += char(n >> 24);
            chunk += xmp;
            if (n & 1) {
                chunk.push_back('\0');
            }
            file += chunk;
            const uint32_t riff = uint32_t(file.size() - 8);
            file[4] = char(riff);
            file[5] = char(riff >> 8);
            file[6] = char(riff >> 16);
            file[7] = char(riff >> 24);
        }
        return file;
    }

    // A TIFF of a 1×1 gray image whose IFD0 and Exif IFD are the camera's
    std::string tiff_with(bool little, const std::string& xmp = {}) {
        auto build = [&](uint32_t strip) {
            ExifBuilder b = camera(little, 2);
            b.longs(ExifBuilder::zero, 256, {1});
            b.longs(ExifBuilder::zero, 257, {1});
            b.shorts(ExifBuilder::zero, 258, {8});
            b.shorts(ExifBuilder::zero, 259, {1});
            b.shorts(ExifBuilder::zero, 262, {1});
            b.longs(ExifBuilder::zero, 273, {strip});
            b.shorts(ExifBuilder::zero, 277, {1});
            b.longs(ExifBuilder::zero, 278, {1});
            b.longs(ExifBuilder::zero, 279, {1});
            if (!xmp.empty()) {
                b.bytes(ExifBuilder::zero, 700, xmp);
            }
            return b.build();
        };
        const std::string first = build(0);
        return build(uint32_t(first.size())) + '\x80';
    }

    std::string box(const std::string& type, const std::string& body) {
        const uint32_t n = uint32_t(body.size() + 8);
        std::string out;
        out += char(n >> 24);
        out += char(n >> 16);
        out += char(n >> 8);
        out += char(n);
        return out + type + body;
    }

    std::string be16(uint32_t v) {
        return std::string{char(v >> 8), char(v)};
    }

    std::string be32(uint32_t v) {
        return std::string{char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    }

    // A HEIF container of no image: ftyp, then meta with an Exif item in
    // the file (construction 0, in two extents) and an XMP item in idat
    // (construction 1)
    std::string heif_with(const std::string& exif, const std::string& xmp) {
        const std::string ftyp = box("ftyp", std::string("heic") + be32(0) + "mif1heic");
        const std::string hdlr = box("hdlr", be32(0) + be32(0) + "pict" + std::string(12, '\0') + '\0');
        const std::string infe1 = box("infe", std::string("\x02\0\0\0", 4) + be16(1) + be16(0) + "Exif" + '\0');
        const std::string infe2 = box("infe", std::string("\x02\0\0\0", 4) + be16(2) + be16(0) + "mime" + "XMP" + '\0' + "application/rdf+xml" + '\0');
        const std::string iinf = box("iinf", be32(0) + be16(2) + infe1 + infe2);
        // the Exif item: 4 bytes of offset (6, past "Exif\0\0"), then the block
        const std::string exif_item = be32(6) + std::string("Exif\0\0", 6) + exif;
        const std::string idat = box("idat", xmp);
        // iloc v1: offset 4 bytes, length 4, base 0, index 0
        auto iloc_of = [&](uint32_t exif_at) {
            std::string b = std::string("\x01\0\0\0", 4) + char(0x44) + char(0x00) + be16(2);
            const uint32_t half = uint32_t(exif_item.size() / 2);
            b += be16(1) + be16(0) + be16(0) + be16(2) + be32(exif_at) + be32(half) + be32(exif_at + half) + be32(uint32_t(exif_item.size() - half));
            b += be16(2) + be16(1) + be16(0) + be16(1) + be32(0) + be32(uint32_t(xmp.size()));
            return box("iloc", b);
        };
        auto meta_of = [&](uint32_t exif_at) { return box("meta", be32(0) + hdlr + iinf + iloc_of(exif_at) + idat); };
        const uint32_t at = uint32_t(ftyp.size() + meta_of(0).size() + 8);
        return ftyp + meta_of(at) + box("mdat", exif_item);
    }

    const std::string Xmp = R"(<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:tiff="http://ns.adobe.com/tiff/1.0/"
    xmlns:exif="http://ns.adobe.com/exif/1.0/"
    xmlns:exifEX="http://cipa.jp/exif/1.0/"
    xmlns:aux="http://ns.adobe.com/exif/1.0/aux/"
    xmlns:xmp="http://ns.adobe.com/xap/1.0/"
    xmlns:photoshop="http://ns.adobe.com/photoshop/1.0/"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    tiff:Make="Nikon"
    tiff:Model="Z 9"
    tiff:Orientation="8"
    exif:ExposureTime="1/1000"
    exif:FNumber="56/10"
    exif:FocalLength="400/1"
    exif:FocalLengthIn35mmFilm="400"
    exif:PixelXDimension="8256"
    exif:PixelYDimension="5504"
    exif:GPSLatitude="52,13.5N"
    exif:GPSLongitude="21,0,36W"
    exif:GPSAltitude="1105/10"
    exif:GPSAltitudeRef="0"
    exif:GPSTimeStamp="2023-08-01T10:15:30Z"
    exif:ExposureBiasValue="+1/3"
    aux:Lens="NIKKOR Z 400mm f/2.8 TC VR S"
    xmp:CreatorTool="Adobe Lightroom 13"
    xmp:CreateDate="2023-08-01T12:15:30.5+02:00"
    xmp:ModifyDate="2023-08-02T09:00:00"
    xmp:Rating="4"
    photoshop:DateCreated="2023-08-01T12:15:30+02:00">
   <exif:ISOSpeedRatings>
    <rdf:Seq><rdf:li>3200</rdf:li></rdf:Seq>
   </exif:ISOSpeedRatings>
   <exif:Flash rdf:parseType="Resource">
    <exif:Fired>False</exif:Fired>
    <exif:Mode>2</exif:Mode>
   </exif:Flash>
   <dc:creator><rdf:Seq><rdf:li>Anna Nowak</rdf:li><rdf:li>Second</rdf:li></rdf:Seq></dc:creator>
   <dc:rights><rdf:Alt><rdf:li xml:lang="pl">Prawa</rdf:li><rdf:li xml:lang="x-default">All rights</rdf:li></rdf:Alt></dc:rights>
   <dc:description><rdf:Alt><rdf:li xml:lang="x-default">A heron</rdf:li></rdf:Alt></dc:description>
   <exifEX:LensMake>Nikon</exifEX:LensMake>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
<?xpacket end="w"?>)";

    void expect_xmp(const codec::metadata& m) {
        EXPECT_EQ(m.make(), optional<string>("Nikon"));
        EXPECT_EQ(m.model(), optional<string>("Z 9"));
        EXPECT_EQ(m.orientation(), 8u);
        EXPECT_DOUBLE_EQ(*m.exposure_time(), 0.001);
        EXPECT_DOUBLE_EQ(*m.f_number(), 5.6);
        EXPECT_DOUBLE_EQ(*m.focal_length(), 400);
        EXPECT_EQ(m.focal_length_35mm(), optional<uint32_t>(400u));
        EXPECT_EQ(m.width(), optional<uint32_t>(8256u));
        EXPECT_EQ(m.height(), optional<uint32_t>(5504u));
        EXPECT_EQ(m.iso(), optional<uint32_t>(3200u));
        EXPECT_EQ(m.flash_fired(), optional<bool>(false));
        EXPECT_DOUBLE_EQ(*m.exposure_bias(), 1.0 / 3);
        EXPECT_EQ(m.lens_model(), optional<string>("NIKKOR Z 400mm f/2.8 TC VR S"));
        EXPECT_EQ(m.lens_make(), optional<string>("Nikon"));
        EXPECT_EQ(m.software(), optional<string>("Adobe Lightroom 13"));
        EXPECT_EQ(m.artist(), optional<string>("Anna Nowak; Second"));
        EXPECT_EQ(m.copyright(), optional<string>("All rights"));
        EXPECT_EQ(m.description(), optional<string>("A heron"));
        EXPECT_EQ(m.rating(), optional<int>(4));
        EXPECT_EQ(fmt(*m.date_time_original()), "2023-08-01T12:15:30+02:00");
        EXPECT_EQ(fmt(*m.date_time_digitized()), "2023-08-01T12:15:30.5+02:00");
        EXPECT_EQ(fmt(*m.date_time()), "2023-08-02T09:00:00Z");
        auto place = m.location();
        ASSERT_TRUE(place);
        EXPECT_NEAR(place->latitude, 52 + 13.5 / 60, 1e-12);
        EXPECT_NEAR(place->longitude, -(21 + 36.0 / 3600), 1e-12);
        EXPECT_DOUBLE_EQ(*place->altitude, 110.5);
        EXPECT_EQ(fmt(*m.gps_time()), "2023-08-01T10:15:30Z");
    }
}

TEST(CodecMetadata_Tests, ExifBlockBothByteOrders) {
    for (bool little : {true, false}) {
        const std::string block = camera(little).build();
        const codec::metadata m = codec::metadata::from_exif(view(block));
        expect_camera(m);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(m.exif().data()), m.exif().size()), block);
        EXPECT_TRUE(m.xmp().empty());
        EXPECT_FALSE(m.rating());
    }
}

TEST(CodecMetadata_Tests, ContainersAndImageIO) {
    const std::string block = camera(true).build();
    const std::string big = camera(false).build();
    struct File {
        const char* name;
        std::string data;
    };
    const File files[] = {
        {"camera.jpg", jpeg_with(block, {})},
        {"camera_be.jpg", jpeg_with(big, {})},
        {"camera.png", png_with(block, {}, false)},
        {"camera.webp", webp_with(block, {})},
        {"camera_le.tif", tiff_with(true)},
        {"camera_be.tif", tiff_with(false)},
        {"camera.heic", heif_with(block, {})},
    };
    for (const auto& f : files) {
        SCOPED_TRACE(f.name);
        auto m = codec::metadata::read(view(f.data));
        ASSERT_TRUE(m) << m.error().message();
        expect_camera(*m);
        if (std::string(f.name).find(".tif") == std::string::npos) {
            EXPECT_FALSE(m->exif().empty());
        } else {
            EXPECT_TRUE(m->exif().empty());   // a TIFF's EXIF is the file
        }
        // through a stream and from a path, the same
        io::buffer in(view(f.data));
        auto streamed = codec::metadata::read(in);
        ASSERT_TRUE(streamed);
        expect_camera(*streamed);
        const std::string path = save_bytes(f.name, f.data);
        auto loaded = codec::metadata::load(string(path.c_str()));
        ASSERT_TRUE(loaded);
        expect_camera(*loaded);
        if (std::string(f.name) != "camera.heic") {   // ImageIO takes no HEIF without an image
            expect_as_imageio(path, *loaded);
        }
    }
    // the image's own block, as a decoder kept it
    codec::image back = codec::jpeg::decode(view(files[0].data)).value();
    expect_camera(codec::metadata::from_exif(back.exif()));
}

TEST(CodecMetadata_Tests, HeifWrittenByImageIO) {
    // HEIC that ImageIO itself writes (sips), its EXIF and XMP items read
    // and held against ImageIO's reading of them
#if defined(__APPLE__)
    if (std::system("command -v sips > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no sips";
    }
    const std::string sources[] = {save_bytes("for_heic_camera.jpg", jpeg_with(camera(true, 2).build(), {})),
                                   save_bytes("for_heic_xmp.jpg", jpeg_with({}, Xmp))};
    int read = 0;
    for (const auto& src : sources) {
        const std::string out = src + ".heic";
        const std::string cmd = "sips -s format heic '" + src + "' --out '" + out + "' > /dev/null 2>&1";
        if (std::system(cmd.c_str()) != 0 || !std::filesystem::exists(out)) {
            continue;   // a system without an HEVC encoder
        }
        SCOPED_TRACE(out);
        auto m = codec::metadata::load(string(out.c_str()));
        ASSERT_TRUE(m) << m.error().message();
        EXPECT_TRUE(m->make());
        EXPECT_TRUE(m->model());
        EXPECT_TRUE(m->date_time_original());
        expect_as_imageio(out, *m);
        ++read;
    }
    if (read == 0) {
        GTEST_SKIP() << "sips writes no HEIC here";
    }
#else
    GTEST_SKIP() << "macOS only";
#endif
}

TEST(CodecMetadata_Tests, XmpInEveryContainer) {
    struct File {
        const char* name;
        std::string data;
    };
    const File files[] = {
        {"xmp.jpg", jpeg_with({}, Xmp)},
        {"xmp.png", png_with({}, Xmp, false)},
        {"xmpz.png", png_with({}, Xmp, true)},
        {"xmp.webp", webp_with({}, Xmp)},
        {"xmp.tif", [] {
             // a TIFF whose only fields are the image's, XMP in tag 700
             ExifBuilder b(true);
             b.bytes(ExifBuilder::zero, 700, Xmp, 7);
             return b.build();
         }()},
        {"xmp.heic", heif_with(camera(true).build(), Xmp)},
    };
    for (const auto& f : files) {
        SCOPED_TRACE(f.name);
        auto m = codec::metadata::read(view(f.data));
        ASSERT_TRUE(m) << m.error().message();
        EXPECT_EQ(std::string(m->xmp().data(), m->xmp().size()), Xmp);
        if (std::string(f.name) == "xmp.heic") {
            expect_camera(*m);   // EXIF first, XMP only where EXIF has nothing
            EXPECT_EQ(m->rating(), optional<int>(4));
            EXPECT_EQ(m->focal_length_35mm(), optional<uint32_t>(105u));
            continue;
        }
        expect_xmp(*m);
        if (std::string(f.name) != "xmp.tif") {
            expect_as_imageio(save_bytes(f.name, f.data), *m);
        }
    }
    // from_exif with a packet
    expect_xmp(codec::metadata::from_exif({}, string(Xmp.c_str())));
}

TEST(CodecMetadata_Tests, ExifFirstThenXmp) {
    ExifBuilder b(true);
    b.ascii(ExifBuilder::zero, 0x010F, "FromExif");
    b.rationals(ExifBuilder::exif, 0x829D, {{4, 1}});
    const codec::metadata m = codec::metadata::from_exif(view(b.build()), string(Xmp.c_str()));
    EXPECT_EQ(m.make(), optional<string>("FromExif"));
    EXPECT_DOUBLE_EQ(*m.f_number(), 4);
    EXPECT_EQ(m.model(), optional<string>("Z 9"));                        // XMP fills
    EXPECT_DOUBLE_EQ(*m.exposure_time(), 0.001);
    EXPECT_EQ(m.orientation(), 8u);
    // XMP in its element form, Bag, a coordinate as a decimal, APEX values
    const std::string elements = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/"><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description xmlns:exif="http://ns.adobe.com/exif/1.0/" xmlns:exifEX="http://cipa.jp/exif/1.0/" xmlns:tiff="http://ns.adobe.com/tiff/1.0/" xmlns:dc="http://purl.org/dc/elements/1.1/">
<tiff:Make>Fujifilm</tiff:Make><exif:ShutterSpeedValue>8</exif:ShutterSpeedValue><exif:ApertureValue>4</exif:ApertureValue>
<exifEX:PhotographicSensitivity>160</exifEX:PhotographicSensitivity>
<exif:GPSLatitude>-12.5</exif:GPSLatitude><exif:GPSLongitude>45.25</exif:GPSLongitude>
<dc:creator><rdf:Bag><rdf:li>Bag First</rdf:li></rdf:Bag></dc:creator>
<exif:Flash><rdf:Description exif:Fired="True"/></exif:Flash>
<exif:DateTimeOriginal>2020-02-29T23:59:59-05:30</exif:DateTimeOriginal>
</rdf:Description></rdf:RDF></x:xmpmeta>)";
    const codec::metadata e = codec::metadata::from_exif({}, string(elements.c_str()));
    EXPECT_EQ(e.make(), optional<string>("Fujifilm"));
    EXPECT_DOUBLE_EQ(*e.exposure_time(), 1.0 / 256);
    EXPECT_DOUBLE_EQ(*e.f_number(), 4);
    EXPECT_EQ(e.iso(), optional<uint32_t>(160u));
    EXPECT_DOUBLE_EQ(e.location()->latitude, -12.5);
    EXPECT_DOUBLE_EQ(e.location()->longitude, 45.25);
    EXPECT_FALSE(e.location()->altitude);
    EXPECT_EQ(e.artist(), optional<string>("Bag First"));
    EXPECT_EQ(e.flash_fired(), optional<bool>(true));
    EXPECT_EQ(fmt(*e.date_time_original()), "2020-02-29T23:59:59-05:30");
}

TEST(CodecMetadata_Tests, SystemPictures) {
    // the system's own files that carry EXIF and XMP, against ImageIO
    const char* paths[] = {
        "/System/Library/CoreServices/DefaultBackground.jpg",
        "/System/Library/CoreServices/DefaultDesktop.heic",
        "/System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/Lock.jpg",
        "/opt/homebrew/share/flutter/examples/image_list/images/coast.jpg",
    };
    const std::string pngsuite = oracle_path("pngsuite/exif2c08.png");   // eXIf of PngSuite
    int read = 0;
    if (std::filesystem::exists(pngsuite)) {
        auto m = codec::metadata::load(string(pngsuite.c_str()));
        ASSERT_TRUE(m);
        EXPECT_EQ(m->copyright(), optional<string>("2017 Willem van Schaik"));
        expect_as_imageio(pngsuite, *m);
        ++read;
    }
    for (const char* path : paths) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        SCOPED_TRACE(path);
        auto m = codec::metadata::load(string(path));
        ASSERT_TRUE(m) << m.error().message();
        expect_as_imageio(path, *m);
        ++read;
    }
    if (read == 0) {
        GTEST_SKIP() << "none of the system's pictures";
    }
}

TEST(CodecMetadata_Tests, DefaultAndEmpty) {
    const codec::metadata none;
    EXPECT_FALSE(none.make());
    EXPECT_FALSE(none.date_time_original());
    EXPECT_FALSE(none.location());
    EXPECT_FALSE(none.gps_time());
    EXPECT_FALSE(none.iso());
    EXPECT_FALSE(none.rating());
    EXPECT_EQ(none.orientation(), 1u);
    EXPECT_TRUE(none.exif().empty());
    EXPECT_TRUE(none.xmp().empty());
    // a copy shares; a moved-from value is still the value (one word)
    codec::metadata a = codec::metadata::from_exif(view(camera(true).build()));
    codec::metadata b = a;
    codec::metadata c = std::move(a);
    EXPECT_EQ(b.make(), c.make());
    EXPECT_EQ(a.make(), c.make());
    // files of no metadata, and bytes of no format
    EXPECT_FALSE(codec::metadata::read(view(text_of(*codec::png::encode(codec::image(4, 4, pixel_format::gray8)))))->make());
    EXPECT_FALSE(codec::metadata::read(view(text_of(*codec::gif::encode(codec::image(4, 4, pixel_format::rgb8)))))->make());
    EXPECT_FALSE(codec::metadata::read(view(text_of(*codec::bmp::encode(codec::image(4, 4, pixel_format::rgb8)))))->make());
    EXPECT_EQ(codec::metadata::read(slice<const byte>()).error().code(), codec::errc::unsupported);
    EXPECT_EQ(codec::metadata::read(view("hello, world")).error().code(), codec::errc::unsupported);
    EXPECT_EQ(codec::metadata::load("/nonexistent/sgcl/photo.jpg").error().code(), codec::errc::io);
    // empty blocks
    EXPECT_FALSE(codec::metadata::from_exif({}).make());
    EXPECT_FALSE(codec::metadata::from_exif(view("II*")).make());
    EXPECT_FALSE(codec::metadata::from_exif({}, string("<not xml")).make());
    EXPECT_FALSE(codec::metadata::from_exif({}, string("<a/>")).make());
}

TEST(CodecMetadata_Tests, MalformedFields) {
    ExifBuilder b(true);
    b.ascii(ExifBuilder::zero, 0x010F, "   ");                          // blank: absent
    b.ascii(ExifBuilder::zero, 0x0110, std::string("A\xE9t\xE9", 4));   // Latin-1, not UTF-8
    b.shorts(ExifBuilder::zero, 0x0112, {9});                           // outside 1..8: 1
    b.ascii(ExifBuilder::zero, 0x0132, "0000:00:00 00:00:00");          // zeros: absent
    b.rationals(ExifBuilder::exif, 0x829A, {{1, 0}});                   // denominator 0: absent
    b.rationals(ExifBuilder::exif, 0x829D, {{0, 10}});                  // f/0: absent
    b.ascii(ExifBuilder::exif, 0x9003, "2023:02:29 10:00:00");          // no 29 February in 2023
    b.ascii(ExifBuilder::exif, 0x9004, "2024:12:31 23:59:60");          // a leap second: :59
    b.ascii(ExifBuilder::exif, 0x9012, "+25:00");                       // no such offset: UTC
    b.shorts(ExifBuilder::exif, 0x8827, {65535});                       // "65535 or more"
    b.longs(ExifBuilder::exif, 0x8833, {102400});                       // ... the exact value
    b.raw(ExifBuilder::exif, 0xA434, 2, 0x7FFFFFFF, 0x10);              // a count past the block
    b.raw(ExifBuilder::exif, 0xA433, 99, 1, 0);                         // a type that is none
    b.rationals(ExifBuilder::gps, 0x0002, {{91, 1}, {0, 1}, {0, 1}});   // past 90°: no place
    b.rationals(ExifBuilder::gps, 0x0004, {{10, 1}, {0, 1}, {0, 1}});
    const codec::metadata m = codec::metadata::from_exif(view(b.build()));
    EXPECT_FALSE(m.make());
    EXPECT_EQ(m.model(), optional<string>("Aété"));
    EXPECT_EQ(m.orientation(), 1u);
    EXPECT_FALSE(m.date_time());
    EXPECT_FALSE(m.exposure_time());
    EXPECT_FALSE(m.f_number());
    EXPECT_FALSE(m.date_time_original());
    EXPECT_EQ(fmt(*m.date_time_digitized()), "2024-12-31T23:59:59Z");
    EXPECT_EQ(m.iso(), optional<uint32_t>(102400u));
    EXPECT_FALSE(m.lens_model());
    EXPECT_FALSE(m.lens_make());
    EXPECT_FALSE(m.location());

    // pointers that loop back to IFD0, or point past the block
    ExifBuilder loop(false);
    loop.ascii(ExifBuilder::zero, 0x010F, "Loop");
    loop.raw(ExifBuilder::zero, 0x8769, 4, 1, 8);
    loop.raw(ExifBuilder::zero, 0x8825, 4, 1, 0xFFFFFFF0u);
    const codec::metadata l = codec::metadata::from_exif(view(loop.build()));
    EXPECT_EQ(l.make(), optional<string>("Loop"));
    EXPECT_FALSE(l.location());
    // a pointer of a type no pointer has (a DOUBLE of any value: the fuzz
    // find), and huge numbers where counts are asked
    ExifBuilder odd(true);
    odd.ascii(ExifBuilder::zero, 0x010F, "Odd");
    odd.raw(ExifBuilder::zero, 0x8769, 12, 1, 8);
    odd.raw(ExifBuilder::zero, 0x8825, 11, 1, 0x7F7FFFFFu);
    const codec::metadata o = codec::metadata::from_exif(view(odd.build()));
    EXPECT_EQ(o.make(), optional<string>("Odd"));
    EXPECT_FALSE(o.iso());
    EXPECT_FALSE(o.location());
    ExifBuilder huge(true);
    huge.raw(ExifBuilder::exif, 0x8827, 11, 1, 0x7F7FFFFFu);   // FLOAT 3.4e38
    huge.raw(ExifBuilder::exif, 0x9209, 11, 1, 0xFF7FFFFFu);   // -3.4e38
    huge.raw(ExifBuilder::exif, 0xA405, 9, 1, 0x80000000u);    // SLONG negative
    const codec::metadata h = codec::metadata::from_exif(view(huge.build()));
    EXPECT_FALSE(h.iso());
    EXPECT_FALSE(h.flash_fired());
    EXPECT_FALSE(h.focal_length_35mm());
    // the camera's block cut at every byte: no crash, a prefix of the fields
    const std::string whole = camera(true).build();
    for (size_t n = 0; n <= whole.size(); ++n) {
        const codec::metadata cut = codec::metadata::from_exif(view(whole.substr(0, n)));
        if (cut.make()) {
            EXPECT_EQ(cut.make(), optional<string>("Canon"));
        }
    }
    EXPECT_TRUE(codec::metadata::from_exif(view(whole)).location());
}

TEST(CodecMetadata_Tests, BrokenContainersAndLimits) {
    const std::string block = camera(true).build();
    const std::string files[] = {jpeg_with(block, Xmp), png_with(block, Xmp, true), webp_with(block, Xmp), heif_with(block, Xmp),
                                 tiff_with(true, Xmp)};
    for (const auto& file : files) {
        // every prefix reads without an error past the signature, what is
        // whole of it read
        for (size_t n = 16; n < file.size(); n += 7) {
            auto m = codec::metadata::read(view(file.substr(0, n)));
            if (m && m->make()) {
                // EXIF's, or XMP's where the cut took EXIF's away
                EXPECT_TRUE(m->make() == optional<string>("Canon") || m->make() == optional<string>("Nikon"));
            }
        }
        // a limit below the blocks: too_large
        auto small = codec::metadata::read(view(file), {.max_metadata = 100});
        ASSERT_FALSE(small);
        EXPECT_EQ(small.error().code(), codec::errc::too_large);
        // the whole file, read
        auto m = codec::metadata::read(view(file));
        ASSERT_TRUE(m);
        expect_camera(*m);
    }
    // a stream past its limit
    io::buffer in(view(files[0]));
    auto limited = codec::metadata::read(in, {.max_pixels = 0, .max_metadata = 1 << 20});
    ASSERT_TRUE(limited);   // 64 MB of room past the pixels
}
