//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The tar reader and writer against Go's archive/tar (its test data, read
// as Go reads it; its writer's golden files, byte for byte), Python's
// tarfile (archives it makes read as it reads them, and ours read by it)
// and the system's tar (bsdtar).
#include "common.h"

#include <bzlib.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>

using namespace compress_test;
namespace tar = compress::tar;
using sgcl::time::datetime;
using sgcl::time::zone;

// Entries and their data: in a vector of the library, as an entry holds
// tracked words (strings), which live on a stack or in a managed object
using Entries = sgcl::vector<std::pair<tar::entry, std::string>>;

namespace {
    // Go's test data: base64 and bzip2 files decoded
    std::string base64_decode(const std::string& in) {
        auto value = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        std::string out;
        uint32_t acc = 0;
        int bits = 0;
        for (char c : in) {
            int v = value(c);
            if (v < 0) {
                continue;
            }
            acc = acc << 6 | uint32_t(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out += char((acc >> bits) & 0xFF);
            }
        }
        return out;
    }

    std::string bunzip2(const std::string& in) {
        std::string out(1 << 20, 0);
        for (;;) {
            unsigned n = unsigned(out.size());
            int st = BZ2_bzBuffToBuffDecompress(out.data(), &n, const_cast<char*>(in.data()), unsigned(in.size()), 0, 0);
            if (st == BZ_OUTBUFF_FULL) {
                out.resize(out.size() * 4);
                continue;
            }
            out.resize(st == BZ_OK ? n : 0);
            return out;
        }
    }

    std::string go_tar(const std::string& name) {
        auto data = read_oracle("tar/" + name);
        if (name.ends_with(".base64")) {
            return base64_decode(data);
        }
        if (name.ends_with(".bz2")) {
            return bunzip2(data);
        }
        return data;
    }

    // Written as the oracles' dumpers write it (esc there)
    std::string esc(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (c < 0x20 || c >= 0x7f || c == '"' || c == '\\' || c == '|' || c == ',' || c == '=') {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            } else {
                out += char(c);
            }
        }
        return out;
    }

    std::string times(const sgcl::optional<datetime>& t) {
        return t ? std::to_string(t->unix_nano()) : std::string("-");
    }

    const char* errc_name(compress::errc c) {
        switch (c) {
            case compress::errc::corrupt: return "corrupt";
            case compress::errc::checksum: return "checksum";
            case compress::errc::unexpected_end: return "unexpected_end";
            case compress::errc::unsupported: return "unsupported";
            case compress::errc::too_large: return "too_large";
            case compress::errc::invalid_header: return "invalid_header";
            case compress::errc::invalid_argument: return "invalid_argument";
            case compress::errc::dictionary_required: return "dictionary_required";
            case compress::errc::io: return "io";
            case compress::errc::password_required: return "password_required";
            case compress::errc::wrong_password: return "wrong_password";
        }
        return "?";
    }

    // One line an entry, as the dumpers of the oracles write them: the
    // kind as its ustar flag, the mode in octal, the times in nanoseconds,
    // the data's length and CRC-32, the pax records sorted by key
    std::string line_of(const tar::entry& e, const std::string& data) {
        std::vector<std::pair<std::string, std::string>> pax;
        for (auto& [k, v] : e.pax) {
            pax.push_back({std::string(k.view()), std::string(v.view())});
        }
        std::sort(pax.begin(), pax.end());
        std::string joined;
        for (auto& [k, v] : pax) {
            joined += (joined.empty() ? "" : ",") + esc(k) + "=" + esc(v);
        }
        static const char flags[] = {'0', '5', '2', '1', '3', '4', '6'};
        char mode[16];
        std::snprintf(mode, sizeof mode, "%o", unsigned(e.mode));
        char crc[16];
        std::snprintf(crc, sizeof crc, "%08lx", crc32(0, reinterpret_cast<const Bytef*>(data.data()), uInt(data.size())));
        return esc(e.name.view()) + "|" + esc(e.link_name.view()) + "|" + flags[size_t(e.type)] + "|" + std::to_string(e.size) + "|" + mode + "|" + std::to_string(e.uid) + "|" +
               std::to_string(e.gid) + "|" + esc(e.user_name.view()) + "|" + esc(e.group_name.view()) + "|" + std::to_string(e.modified.unix_nano()) + "|" + times(e.accessed) + "|" +
               times(e.changed) + "|" + std::to_string(e.dev_major) + "|" + std::to_string(e.dev_minor) + "|" + std::to_string(data.size()) + "|" + crc + "|" + joined;
    }

    // Every entry's line, the data read in pieces of `piece` bytes (none
    // of an entry past 16 MiB, as the Go dumper); "UNSUPPORTED" for an
    // entry the reader steps over, "END" or "ERR <code>" last
    std::string dump(tar::reader& r, size_t piece = 1 << 16) {
        std::string out;
        for (;;) {
            auto e = r.next();
            if (!e) {
                if (e.error().code() == compress::errc::unsupported) {
                    out += "UNSUPPORTED\n";
                    continue;
                }
                out += std::string("ERR ") + errc_name(e.error().code()) + "\n";
                break;
            }
            if (!*e) {
                out += "END\n";
                break;
            }
            std::string data;
            if ((*e)->size <= (1 << 24)) {
                std::string buf(piece, 0);
                for (;;) {
                    auto n = r.read(sgcl::slice<std::byte>(reinterpret_cast<std::byte*>(buf.data()), buf.size()));
                    if (!n) {
                        out += line_of(**e, data) + "\n";
                        out += std::string("READERR ") + errc_name(r.last_error()->code()) + "\n";
                        return out;
                    }
                    if (*n == 0) {
                        break;
                    }
                    data.append(buf.data(), *n);
                }
            }
            out += line_of(**e, data) + "\n";
        }
        return out;
    }

    std::string dump(const std::string& archive, size_t feed = 1 << 20, size_t piece = 1 << 16) {
        tar::reader r(dribble{archive, feed});
        return dump(r, piece);
    }

    std::string bytes_of(const sgcl::io::buffer& b) {
        return std::string(reinterpret_cast<const char*>(b.data().data()), b.size());
    }

    std::string temp_path(const std::string& name) {
        const char* t = std::getenv("TMPDIR");
        return std::string(t ? t : "/tmp") + "/" + name;
    }

    std::string read_file(const std::string& path) {
        std::ifstream is(path, std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    void write_file(const std::string& path, const std::string& data) {
        std::ofstream os(path, std::ios::binary);
        os << data;
    }

    // A command's standard output, and whether it ended with status 0
    bool run(const std::string& cmd, std::string& out) {
        out.clear();
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return false;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        return pclose(p) == 0;
    }

    // Python's tarfile as the oracle: `make` writes an archive of every kind
    // of entry in a format, `dump` prints an archive as tarfile reads it, in
    // the lines of line_of
    const char* python_oracle = R"PY(
import io, random, sys, tarfile, zlib

KNOWN = {'path', 'linkpath', 'size', 'uid', 'gid', 'uname', 'gname', 'mtime', 'atime', 'ctime'}
FLAGS = {tarfile.REGTYPE: '0', tarfile.AREGTYPE: '0', tarfile.CONTTYPE: '0', tarfile.DIRTYPE: '5', tarfile.SYMTYPE: '2',
         tarfile.LNKTYPE: '1', tarfile.CHRTYPE: '3', tarfile.BLKTYPE: '4', tarfile.FIFOTYPE: '6'}

def esc(s):
    b = s.encode('utf-8', 'surrogateescape') if isinstance(s, str) else s
    return ''.join('\\x%02x' % c if c < 0x20 or c >= 0x7f or chr(c) in '"\\|,=' else chr(c) for c in b)

def ns(s):
    whole, _, frac = s.partition('.')
    n = int(whole) * 10**9
    f = int((frac + '000000000')[:9]) if frac else 0
    return n - f if whole.startswith('-') else n + f

def dump(path):
    with tarfile.open(path, encoding='utf-8', errors='surrogateescape') as t:
        for m in t:
            data = t.extractfile(m).read() if m.isreg() else b''
            p = m.pax_headers
            mtime = ns(p['mtime']) if 'mtime' in p else int(m.mtime) * 10**9
            atime = str(ns(p['atime'])) if 'atime' in p else '-'
            ctime = str(ns(p['ctime'])) if 'ctime' in p else '-'
            pax = ','.join(esc(k) + '=' + esc(v) for k, v in sorted(p.items(), key=lambda kv: kv[0].encode('utf-8', 'surrogateescape')) if k not in KNOWN)
            size = m.size if m.isreg() else 0
            name = m.name + ('/' if m.isdir() and not m.name.endswith('/') else '')
            print('%s|%s|%s|%d|%o|%d|%d|%s|%s|%d|%s|%s|%d|%d|%d|%08x|%s' % (
                esc(name), esc(m.linkname), FLAGS[m.type], size, m.mode & 0o7777, m.uid, m.gid,
                esc(m.uname), esc(m.gname), mtime, atime, ctime, m.devmajor, m.devminor, len(data), zlib.crc32(data), pax))
    print('END')

def make(path, fmt_name):
    fmt = {'ustar': tarfile.USTAR_FORMAT, 'gnu': tarfile.GNU_FORMAT, 'pax': tarfile.PAX_FORMAT}[fmt_name]
    rng = random.Random(7)
    extra = {'pax_headers': {'comment': 'global one'}} if fmt_name == 'pax' else {}
    with tarfile.open(path, 'w', format=fmt, encoding='utf-8', **extra) as t:
        def add(name, kind=tarfile.REGTYPE, data=b'', **kw):
            i = tarfile.TarInfo(name)
            i.type = kind
            i.mode = kw.pop('mode', 0o644)
            i.uid, i.gid = kw.pop('uid', 1000), kw.pop('gid', 100)
            i.uname, i.gname = kw.pop('uname', 'user'), kw.pop('gname', 'staff')
            i.mtime = kw.pop('mtime', 1700000000)
            for k, v in kw.items():
                setattr(i, k, v)
            i.size = len(data)
            t.addfile(i, io.BytesIO(data) if kind == tarfile.REGTYPE else None)

        add('hello.txt', data=b'hello world\n')
        add('dir', tarfile.DIRTYPE, mode=0o755)
        add('dir/empty')
        for n in (1, 511, 512, 513, 70000):
            add('dir/b%d' % n, data=bytes(rng.getrandbits(8) for _ in range(n)))
        add('link', tarfile.SYMTYPE, linkname='hello.txt', mode=0o777)
        add('hard', tarfile.LNKTYPE, linkname='hello.txt')
        add('fifo', tarfile.FIFOTYPE)
        add('null', tarfile.CHRTYPE, devmajor=1, devminor=3)
        add('sda', tarfile.BLKTYPE, devmajor=8, devminor=0)
        add('p' * 120 + '/' + 'n' * 90, data=b'split')
        add('zażółć/gęślą.txt', data=b'utf-8', uname='użytkownik')
        if fmt_name != 'ustar':
            add('long/' + 'x' * 150 + '/' + 'y' * 150 + '/file.txt', data=b'long')
            add('longlink', tarfile.SYMTYPE, linkname='t' * 150)
            add('bigid', uid=3000000, gid=4000000)
            add('negative', mtime=-100)
        if fmt_name == 'pax':
            add('fraction', mtime=1700000000.5)
            add('records', data=b'r', pax_headers={'comment': 'zażółć', 'SCHILY.xattr.user.a': 'b'}, uname='u' * 40)

if sys.argv[1] == 'dump':
    dump(sys.argv[2])
else:
    make(sys.argv[2], sys.argv[3])
)PY";

    std::string python_script() {
        auto path = temp_path("sgcl-tar-oracle.py");
        write_file(path, python_oracle);
        return path;
    }

    tar::entry file_entry(const std::string& name, uint64_t size, int64_t mtime = 1700000000) {
        tar::entry e;
        e.name = sgcl::string(name);
        e.size = size;
        e.mode = sgcl::io::permissions(0644);
        e.modified = datetime::from_unix(mtime, zone::utc());
        return e;
    }

    // Go's reading of its test data, by a program over archive/tar (the
    // oracle's dumper: every entry's header and its data's length and
    // CRC-32), in the lines of line_of. Go's and ours differ only where
    // the reader chose otherwise, noted at the files:
    //   - an entry that is a header alone (a directory, a link, a device)
    //     has size 0, where Go gives the size its header says (hdr-only)
    //   - global pax records apply to the entries after them, as POSIX
    //     says; Go returns a global header as an entry of its own and
    //     applies nothing (pax-global-records)
    //   - a sparse file is unsupported and stepped over; Go expands it
    //     (the gnu-*sparse*, pax-*sparse*, sparse-formats files), and fails
    //     on gnu-sparse-many-zeros, whose map is longer than it takes
    //   - GNU's D (a dump directory) is a directory, as tar lists it, with
    //     no data: its listing of names is stepped over (gnu-incremental)
    const std::map<std::string, std::string> go_reading = {
        {"file-and-dir.tar", R"(small.txt||0|5|0|0|0|||0|-|-|0|0|5|6cbd88fc|
dir/||5|0|0|0|0|||0|-|-|0|0|0|00000000|
END
)"},
        {"gnu-incremental.tar", R"(test2/||5|0|755|1000|1000|rawr|dsnet|1441973427000000000|1441974501000000000|1441973436000000000|0|0|0|00000000|
test2/foo||0|64|644|1000|1000|rawr|dsnet|1441973363000000000|1441974501000000000|1441973436000000000|0|0|64|7fc4a157|
UNSUPPORTED
END
)"},
        {"gnu-long-nul.tar", R"(0123456789||0|0|644|1000|1000|rawr|dsnet|1486082191000000000|-|-|0|0|0|00000000|
END
)"},
        {"gnu-multi-hdrs.tar", R"(GNU2/GNU2/long-path-name|GNU4/GNU4/long-linkpath-name|2|0|0|0|0|||0|-|-|0|0|0|00000000|
END
)"},
        {"gnu-nil-sparse-data.tar", R"(UNSUPPORTED
END
)"},
        {"gnu-nil-sparse-hole.tar", R"(UNSUPPORTED
END
)"},
        {"gnu-not-utf8.tar", R"(hi\x80\x81\x82\x83bye||0|0|644|1000|1000|rawr|dsnet|0|-|-|0|0|0|00000000|
END
)"},
        {"gnu-sparse-big.tar.base64", R"(UNSUPPORTED
END
)"},
        {"gnu-sparse-many-zeros.tar.bz2", R"(UNSUPPORTED
END
)"},
        {"gnu-utf8.tar", R"(\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9||0|0|644|1000|1000|\xe2\x98\xba|\xe2\x9a\xb9|0|-|-|0|0|0|00000000|
END
)"},
        {"gnu.tar", R"(small.txt||0|5|640|73025|5000|dsymonds|eng|1244428340000000000|-|-|0|0|5|6cbd88fc|
small2.txt||0|11|640|73025|5000|dsymonds|eng|1244436044000000000|-|-|0|0|11|ddac04b3|
END
)"},
        {"hardlink.tar", R"(file.txt||0|15|644|1000|100|vbatts|users|1425484303000000000|-|-|0|0|15|7b4b976f|
hard.txt|file.txt|1|0|644|1000|100|vbatts|users|1425484303000000000|-|-|0|0|0|00000000|
END
)"},
        {"hdr-only.tar", R"(dir/||5|0|750|319973|5000|joetsai|eng|1442273732000000000|-|-|0|0|0|00000000|
fifo||6|0|640|319973|5000|joetsai|eng|1442273806000000000|-|-|0|0|0|00000000|
file||0|46|640|319973|5000|joetsai|eng|1442273747000000000|-|-|0|0|46|2d1f99a6|
hardlink|file|1|0|640|319973|5000|joetsai|eng|1442273747000000000|-|-|0|0|0|00000000|
null||3|0|666|319973|5000|joetsai|eng|1442264573000000000|-|-|1|3|0|00000000|
sda||4|0|660|319973|5000|joetsai|eng|1442264573000000000|-|-|8|0|0|00000000|
symlink|file|2|0|777|319973|5000|joetsai|eng|1442273756000000000|-|-|0|0|0|00000000|
badlink|missing|2|0|777|319973|5000|joetsai|eng|1442274044000000000|-|-|0|0|0|00000000|
dir/||5|0|750|319973|5000|joetsai|eng|1442273732000000000|-|-|0|0|0|00000000|
fifo||6|0|640|319973|5000|joetsai|eng|1442273806000000000|-|-|0|0|0|00000000|
file||0|46|640|319973|5000|joetsai|eng|1442273747000000000|-|-|0|0|46|2d1f99a6|
hardlink|file|1|0|640|319973|5000|joetsai|eng|1442273747000000000|-|-|0|0|0|00000000|
null||3|0|666|319973|5000|joetsai|eng|1442264573000000000|-|-|1|3|0|00000000|
sda||4|0|660|319973|5000|joetsai|eng|1442264573000000000|-|-|8|0|0|00000000|
symlink|file|2|0|777|319973|5000|joetsai|eng|1442273756000000000|-|-|0|0|0|00000000|
badlink|missing|2|0|777|319973|5000|joetsai|eng|1442274044000000000|-|-|0|0|0|00000000|
END
)"},
        {"invalid-go17.tar", R"(aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/foo||0|0|0|2097152|0|||0|-|-|0|0|0|00000000|
END
)"},
        {"issue10968.tar", R"(ERR invalid_header
)"},
        {"issue11169.tar", R"(ERR invalid_header
)"},
        {"issue12435.tar", R"(ERR invalid_header
)"},
        {"neg-size.tar.base64", R"(ERR invalid_header
)"},
        {"nil-uid.tar", R"(P1050238.JPG.log||0|14|664|0|0|eyefi|eyefi|1365454838000000000|-|-|0|0|14|3691bebc|
END
)"},
        {"pax-bad-hdr-file.tar", R"(ERR invalid_header
)"},
        {"pax-bad-hdr-large.tar.bz2", R"(ERR too_large
)"},
        {"pax-bad-mtime-file.tar", R"(ERR invalid_header
)"},
        {"pax-global-records.tar", R"(global1||0|0|0|0|0|||1500000000000000000|-|-|0|0|0|00000000|
file2||0|0|0|0|0|||1500000000000000000|-|-|0|0|0|00000000|
file3||0|0|0|0|0|||1500000000000000000|-|-|0|0|0|00000000|
file4||0|0|0|0|0|||1400000000000000000|-|-|0|0|0|00000000|
END
)"},
        {"pax-multi-hdrs.tar", R"(bar|PAX4/PAX4/long-linkpath-name|2|0|0|0|0|||0|-|-|0|0|0|00000000|
END
)"},
        {"pax-nil-sparse-data.tar", R"(UNSUPPORTED
END
)"},
        {"pax-nil-sparse-hole.tar", R"(UNSUPPORTED
END
)"},
        {"pax-nul-path.tar", R"(ERR invalid_header
)"},
        {"pax-nul-xattrs.tar", R"(ERR invalid_header
)"},
        {"pax-path-hdr.tar", R"(END
)"},
        {"pax-pos-size-file.tar", R"(foo||0|999|640|319973|5000|joetsai|eng|1442282516000000000|-|-|0|0|999|5fd7e86a|
END
)"},
        {"pax-records.tar", R"(file||0|0|0|0|0|longlonglonglonglonglonglonglonglonglong||0|-|-|0|0|0|00000000|GOLANG.pkg=tar,comment=Hello\x2c \xe4\xb8\x96\xe7\x95\x8c
END
)"},
        {"pax-sparse-big.tar.base64", R"(UNSUPPORTED
END
)"},
        {"pax.tar", R"(a/123456789101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899100||0|7|664|1000|1000|shane|shane|1350244992023960108|1350244992023960108|1350244992023960108|0|0|7|6820bd93|
a/b|123456789101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899100|2|0|777|1000|1000|shane|shane|1350266320910238425|1350266320910238425|1350266320910238425|0|0|0|00000000|
END
)"},
        {"sparse-formats.tar", R"(UNSUPPORTED
UNSUPPORTED
UNSUPPORTED
UNSUPPORTED
end||0|4|644|1000|1000|david|david|1392398319000000000|-|-|0|0|4|8eb179ba|
END
)"},
        {"star.tar", R"(small.txt||0|5|640|73025|5000|dsymonds|eng|1244592783000000000|1244592783000000000|1244592783000000000|0|0|5|6cbd88fc|
small2.txt||0|11|640|73025|5000|dsymonds|eng|1244592783000000000|1244592783000000000|1244592783000000000|0|0|11|ddac04b3|
END
)"},
        {"trailing-slash.tar", R"(123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/123456789/||5|0|0|0|0|||0|-|-|0|0|0|00000000|
END
)"},
        {"ustar-file-devs.tar", R"(file||0|0|644|0|0|||0|-|-|1|1|0|00000000|
END
)"},
        {"ustar-file-reg.tar", R"(foo||0|684|640|319973|5000|joetsai|eng|1442282516000000000|-|-|0|0|684|50086c36|
END
)"},
        {"ustar.tar", R"(longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/file.txt||0|6|644|501|20|shane|staff|1360135598000000000|-|-|0|0|6|363a3020|
END
)"},
        {"v7.tar", R"(small.txt||0|5|444|73025|5000|||1244593104000000000|-|-|0|0|5|6cbd88fc|
small2.txt||0|11|444|73025|5000|||1244593104000000000|-|-|0|0|11|ddac04b3|
END
)"},
        {"writer-big-long.tar.base64", R"(longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/longname/16gig.txt||0|17179869184|644|1000|1000|guillaume|guillaume|1399583047000000000|-|-|0|0|0|00000000|
ERR unexpected_end
)"},
        {"writer-big.tar", R"(tmp/16gig.txt||0|17179869184|640|73025|5000|dsymonds|eng|1254699560000000000|-|-|0|0|0|00000000|
ERR unexpected_end
)"},
        {"writer.tar", R"(small.txt||0|5|640|73025|5000|dsymonds|eng|1246508266000000000|-|-|0|0|5|6cbd88fc|
small2.txt||0|11|640|73025|5000|dsymonds|eng|1245217492000000000|-|-|0|0|11|ddac04b3|
link.txt|small.txt|2|0|777|1000|1000|strings|strings|1314603082000000000|-|-|0|0|0|00000000|
END
)"},
        {"xattrs.tar", R"(small.txt||0|5|644|1000|10|alex|wheel|1386065770448252320|1389782991419875220|1389782956794414986|0|0|5|6cbd88fc|SCHILY.xattr.security.selinux=unconfined_u:object_r:default_t:s0\x00,SCHILY.xattr.user.key=value,SCHILY.xattr.user.key2=value2
small2.txt||0|11|644|1000|10|alex|wheel|1386065770449252304|1389782991419875220|1386065770449252304|0|0|11|ddac04b3|SCHILY.xattr.security.selinux=unconfined_u:object_r:default_t:s0\x00
END
)"},
    };
}

// Go's test data read as Go reads it, fed a byte, two, three and seven at
// a time and whole, the data read in pieces of 1, 3, 7 and 64 KB
TEST(Tar_Tests, GoTestDataAsGoReadsIt) {
    ASSERT_EQ(go_reading.size(), 44u);
    for (auto& [name, want] : go_reading) {
        auto archive = go_tar(name);
        ASSERT_FALSE(archive.empty()) << name;
        EXPECT_EQ(dump(archive), want) << name;
        for (auto [feed, piece] : {std::pair<size_t, size_t>(1, 1 << 16), {2, 1 << 16}, {3, 7}, {7, 1}, {7, 1 << 16}}) {
            if (archive.size() > (4 << 20) && feed < 7) {
                continue;   // gnu-sparse-many-zeros: 40 MB of a sparse map stepped over
            }
            ASSERT_EQ(dump(archive, feed, piece), want) << name << " feed " << feed << " piece " << piece;
        }
    }
}

// Python's tarfile writes an archive of every kind of entry in each
// format; the reader reads what tarfile reads, fed in small pieces too
TEST(Tar_Tests, PythonArchivesReadAsPythonReadsThem) {
    std::string out;
    if (!run("python3 -c 'import tarfile' 2>/dev/null", out)) {
        GTEST_SKIP() << "no python3";
    }
    auto script = python_script();
    for (auto format : {"ustar", "gnu", "pax"}) {
        auto path = temp_path(std::string("sgcl-tar-py-") + format + ".tar");
        ASSERT_TRUE(run("python3 '" + script + "' make '" + path + "' " + format, out)) << format;
        std::string want;
        ASSERT_TRUE(run("python3 '" + script + "' dump '" + path + "'", want)) << format;
        auto archive = read_file(path);
        EXPECT_EQ(dump(archive), want) << format;
        EXPECT_EQ(dump(archive, 7, 5), want) << format;
        EXPECT_EQ(dump(archive, 1, 1 << 16), want) << format;
    }
}

// Go's writer tests: the same headers and data written make the golden
// files byte for byte (writer-big-long: the headers before its 16 GiB)
TEST(Tar_Tests, GoWriterGoldenFilesByteForByte) {
    auto write = [](const Entries& entries, bool close = true) {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        for (auto& [e, data] : entries) {
            auto h = w.write_header(e);
            EXPECT_TRUE(h) << (h ? "" : std::string(h.error().message().view()));
            if (!data.empty()) {
                EXPECT_TRUE(w.write(bytes(data)));
            }
        }
        if (close) {
            EXPECT_TRUE(w.close());
        }
        return bytes_of(sink);
    };
    auto entry = [](std::string name, tar::kind k, uint64_t size, unsigned mode, int64_t uid, int64_t gid, std::string uname, std::string gname, int64_t mtime, std::string link = "") {
        tar::entry e;
        e.name = sgcl::string(name);
        e.type = k;
        e.size = size;
        e.mode = sgcl::io::permissions(mode);
        e.uid = uid;
        e.gid = gid;
        e.user_name = sgcl::string(uname);
        e.group_name = sgcl::string(gname);
        e.modified = datetime::from_unix(mtime, zone::utc());
        e.link_name = sgcl::string(link);
        return e;
    };
    using K = tar::kind;
    EXPECT_EQ(write({{entry("small.txt", K::file, 5, 0640, 73025, 5000, "dsymonds", "eng", 1246508266), "Kilts"},
                     {entry("small2.txt", K::file, 11, 0640, 73025, 5000, "dsymonds", "eng", 1245217492), "Google.com\n"},
                     {entry("link.txt", K::symlink, 0, 0777, 1000, 1000, "strings", "strings", 1314603082, "small.txt"), ""}}),
              go_tar("writer.tar"));
    std::string longname;
    for (int i = 0; i < 15; ++i) {
        longname += "longname/";
    }
    EXPECT_EQ(write({{entry(longname + "file.txt", K::file, 6, 0644, 501, 20, "shane", "staff", 1360135598), "hello\n"}}), go_tar("ustar.tar"));
    EXPECT_EQ(write({{entry("file.txt", K::file, 15, 0644, 1000, 100, "vbatts", "users", 1425484303), "Slartibartfast\n"},
                     {entry("hard.txt", K::hardlink, 0, 0644, 1000, 100, "vbatts", "users", 1425484303, "file.txt"), ""}}),
              go_tar("hardlink.tar"));
    auto records = entry("file", K::file, 0, 0, 0, 0, "longlonglonglonglonglonglonglonglonglong", "", 0);
    records.pax.push_back({sgcl::string("comment"), sgcl::string("Hello, 世界")});
    records.pax.push_back({sgcl::string("GOLANG.pkg"), sgcl::string("tar")});
    EXPECT_EQ(write({{records, ""}}), go_tar("pax-records.tar"));
    EXPECT_EQ(write({{entry("small.txt", K::file, 5, 0, 0, 0, "", "", 0), "Kilts"}, {entry("dir/", K::directory, 0, 0, 0, 0, "", "", 0), ""}}), go_tar("file-and-dir.tar"));
    std::string slashes;
    for (int i = 0; i < 30; ++i) {
        slashes += "123456789/";
    }
    EXPECT_EQ(write({{entry(slashes, K::directory, 0, 0, 0, 0, "", "", 0), ""}}), go_tar("trailing-slash.tar"));
    auto big = write({{entry(longname + "16gig.txt", K::file, uint64_t(16) << 30, 0644, 1000, 1000, "guillaume", "guillaume", 1399583047), ""}}, false);
    auto golden = go_tar("writer-big-long.tar.base64");
    ASSERT_EQ(golden.size(), 1536u);
    EXPECT_EQ(big, golden);
}

namespace {
    // The entries the writer's oracle tests write: every kind, and every
    // reason for a pax header
    Entries varied_entries() {
        Entries v;
        std::mt19937 rng(11);
        auto add = [&](tar::entry e, std::string data = {}) {
            e.size = e.type == tar::kind::file ? data.size() : 0;
            if (e.user_name.empty()) {
                e.user_name = "user";
            }
            e.group_name = "staff";
            e.uid = e.uid ? e.uid : 1000;
            e.gid = e.gid ? e.gid : 100;
            v.push_back({e, data});
        };
        auto of = [](std::string name, tar::kind k = tar::kind::file, unsigned mode = 0644) {
            auto e = file_entry(name, 0);
            e.type = k;
            e.mode = sgcl::io::permissions(mode);
            return e;
        };
        auto random = [&](size_t n) {
            std::string s(n, 0);
            for (auto& c : s) {
                c = char(rng());
            }
            return s;
        };
        add(of("hello.txt"), "hello world\n");
        add(of("dir/", tar::kind::directory, 0755));
        add(of("dir/empty"));
        for (size_t n : {1, 511, 512, 513, 70000}) {
            add(of("dir/b" + std::to_string(n)), random(n));
        }
        auto link = of("link", tar::kind::symlink, 0777);
        link.link_name = "hello.txt";
        add(link);
        auto hard = of("hard", tar::kind::hardlink);
        hard.link_name = "hello.txt";
        add(hard);
        add(of("fifo", tar::kind::fifo));
        auto null = of("null", tar::kind::char_device, 0666);
        null.dev_major = 1;
        null.dev_minor = 3;
        add(null);
        auto sda = of("sda", tar::kind::block_device, 0660);
        sda.dev_major = 8;
        add(sda);
        add(of(std::string(120, 'p') + "/" + std::string(90, 'n')), "split");
        add(of("long/" + std::string(150, 'x') + "/" + std::string(150, 'y') + "/file.txt"), "long");
        auto utf = of("za\u017c\u00f3\u0142\u0107/g\u0119\u015bl\u0105.txt");
        utf.user_name = "u\u017cytkownik";
        add(utf, "utf-8");
        auto longlink = of("longlink", tar::kind::symlink, 0777);
        longlink.link_name = sgcl::string(std::string(150, 't'));
        add(longlink);
        auto bigid = of("bigid");
        bigid.uid = 3000000;
        bigid.gid = 4000000;
        add(bigid);
        auto negative = of("negative");
        negative.modified = datetime::from_unix(-100, zone::utc());
        add(negative);
        auto fraction = of("fraction");
        fraction.modified = datetime::from_unix_nano(1700000000500000000, zone::utc());
        add(fraction);
        auto beyond = of("beyond");
        beyond.modified = datetime::from_unix(9000000000, zone::utc());   // past eleven octal digits: 2255
        add(beyond);
        auto timed = of("times");
        timed.accessed = datetime::from_unix_nano(1389782991419875220, zone::utc());
        timed.changed = datetime::from_unix_nano(-1500000000, zone::utc());
        add(timed, "t");
        auto records = of("records");
        records.user_name = sgcl::string(std::string(40, 'u'));
        records.pax.push_back({sgcl::string("comment"), sgcl::string("za\u017c\u00f3\u0142\u0107")});
        records.pax.push_back({sgcl::string("SCHILY.xattr.user.a"), sgcl::string(std::string("b\0c", 3))});
        add(records, "r");
        return v;
    }

    std::string write_all(const Entries& entries) {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        for (auto& [e, data] : entries) {
            EXPECT_TRUE(w.write_header(e));
            // in pieces, as a copy would write it
            for (size_t i = 0; i < data.size(); i += 1000) {
                EXPECT_TRUE(w.write(bytes(data.substr(i, 1000))));
            }
        }
        EXPECT_TRUE(w.close());
        return bytes_of(sink);
    }

    // What the reader should give for the entries written
    std::string expected_dump(const Entries& entries) {
        std::string out;
        for (auto& [e, data] : entries) {
            out += line_of(e, data) + "\n";
        }
        return out + "END\n";
    }

    // A header block made by hand: the name, the type flag, the size field
    // as given, the checksum right
    std::string raw_header(const std::string& name, char flag, const std::string& size_field) {
        std::string h(512, '\0');
        h.replace(0, std::min<size_t>(name.size(), 100), name.substr(0, 100));
        h.replace(100, 8, std::string("0000644", 7) + '\0');
        h.replace(124, 12, size_field.substr(0, 12));
        h.replace(136, 12, std::string("00000000000", 11) + '\0');
        h[156] = flag;
        h.replace(257, 8, std::string("ustar\0" "00", 8));
        h.replace(148, 8, "        ");
        unsigned sum = 0;
        for (unsigned char c : h) {
            sum += c;
        }
        char f[8];
        std::snprintf(f, sizeof f, "%06o", sum);
        h.replace(148, 7, std::string(f, 6) + '\0');
        return h;
    }

    std::string octal12(uint64_t v) {
        char f[16];
        std::snprintf(f, sizeof f, "%011llo", (unsigned long long)v);
        return std::string(f, 11) + '\0';
    }

    std::string padded(std::string data) {
        data.resize((data.size() + 511) / 512 * 512, '\0');
        return data;
    }

    std::string pax_record(const std::string& k, const std::string& v) {
        size_t n = k.size() + v.size() + 3;
        size_t digits = std::to_string(n).size();
        n += digits;
        if (std::to_string(n).size() != digits) {
            ++n;
        }
        return std::to_string(n) + " " + k + "=" + v + "\n";
    }

    const std::string end_blocks(1024, '\0');
}

// The writer's archive of every kind of entry, read back by the reader,
// by Python's tarfile and by the system's tar, all seeing the same
TEST(Tar_Tests, WriterReadByPythonAndSystemTar) {
    auto entries = varied_entries();
    auto archive = write_all(entries);
    auto want = expected_dump(entries);
    EXPECT_EQ(dump(archive), want);
    EXPECT_EQ(dump(archive, 3, 7), want);
    std::string out;
    if (!run("python3 -c 'import tarfile' 2>/dev/null", out)) {
        GTEST_SKIP() << "no python3";
    }
    auto path = temp_path("sgcl-tar-ours.tar");
    write_file(path, archive);
    std::string python;
    ASSERT_TRUE(run("python3 '" + python_script() + "' dump '" + path + "'", python));
    EXPECT_EQ(python, want);
    // bsdtar lists every name (the one not ASCII as macOS decomposes it)
    // and unpacks the data
    std::string listing, names;
    ASSERT_TRUE(run("LC_ALL=en_US.UTF-8 tar -tf '" + path + "'", listing));
    for (auto& [e, data] : entries) {
        auto n = std::string(e.name.view());
        names += (n.starts_with("zaż") ? std::string("zażółć/gęślą.txt") : n) + "\n";
    }
    EXPECT_EQ(listing, names);
    std::string content;
    ASSERT_TRUE(run("tar -xOf '" + path + "' dir/b70000", content));
    EXPECT_EQ(content, entries[7].second);
}

// ustar when the entry fits, pax with the records it needs when not (B9)
TEST(Tar_Tests, WriterChoosesUstarOrPax) {
    auto header_of = [](const tar::entry& e) {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        auto h = w.write_header(e);
        EXPECT_TRUE(h) << (h ? "" : std::string(h.error().message().view()));
        return bytes_of(sink);
    };
    auto records_of = [](const std::string& out) -> std::string {
        if (out.size() < 1024 || out[156] != 'x') {
            return "";
        }
        auto size = std::stoull(out.substr(124, 11), nullptr, 8);
        return out.substr(512, size);
    };
    auto fits = file_entry("a.txt", 10);
    EXPECT_EQ(header_of(fits).size(), 512u);
    auto split = file_entry(std::string(150, 'd') + "/" + std::string(100, 'f'), 0);
    EXPECT_EQ(header_of(split).size(), 512u);   // a prefix and a name
    struct Case {
        const char* why;
        std::function<void(tar::entry&)> change;
        std::string record;
    };
    std::vector<Case> cases = {
        {"name past ustar", [](tar::entry& e) { e.name = sgcl::string(std::string(160, 'n')); }, " path=" + std::string(160, 'n') + "\n"},
        {"name not ASCII", [](tar::entry& e) { e.name = "\u00e9.txt"; }, " path=\u00e9.txt\n"},
        {"link past ustar", [](tar::entry& e) { e.type = tar::kind::symlink; e.size = 0; e.link_name = sgcl::string(std::string(101, 'l')); }, " linkpath="},
        {"user not ASCII", [](tar::entry& e) { e.user_name = "\u0142"; }, " uname=\u0142\n"},
        {"group past 32", [](tar::entry& e) { e.group_name = sgcl::string(std::string(33, 'g')); }, " gname="},
        {"uid past 2097151", [](tar::entry& e) { e.uid = 2097152; }, " uid=2097152\n"},
        {"gid negative", [](tar::entry& e) { e.gid = -1; }, " gid=-1\n"},
        {"size of 8 GiB", [](tar::entry& e) { e.size = uint64_t(8) << 30; }, " size=8589934592\n"},
        {"a fraction of a second", [](tar::entry& e) { e.modified = datetime::from_unix_nano(1500000000123000000, zone::utc()); }, " mtime=1500000000.123\n"},
        {"before 1970", [](tar::entry& e) { e.modified = datetime::from_unix_nano(-1500000000, zone::utc()); }, " mtime=-1.5\n"},
        {"past eleven octal digits", [](tar::entry& e) { e.modified = datetime::from_unix(8589934592, zone::utc()); }, " mtime=8589934592\n"},
        {"an access time", [](tar::entry& e) { e.accessed = datetime::from_unix(5, zone::utc()); }, " atime=5\n"},
        {"a change time", [](tar::entry& e) { e.changed = datetime::from_unix(6, zone::utc()); }, " ctime=6\n"},
        {"records of its own", [](tar::entry& e) { e.pax.push_back({sgcl::string("comment"), sgcl::string("hi")}); }, " comment=hi\n"},
    };
    for (auto& c : cases) {
        auto e = file_entry("a.txt", 0);
        c.change(e);
        auto out = header_of(e);
        auto records = records_of(out);
        EXPECT_NE(records.find(c.record), std::string::npos) << c.why << ": " << records;
        // and read back as it was
        tar::reader r(dribble{out + end_blocks, 5});
        auto back = r.next();
        ASSERT_TRUE(back && *back) << c.why;
        EXPECT_EQ(**back, e) << c.why;
    }
    // the largest time in eleven octal digits fits
    auto edge = file_entry("a.txt", 0, 8589934591);
    EXPECT_EQ(header_of(edge).size(), 512u);
}

// What the format cannot hold, and the data of an entry not its size, are
// invalid_argument, naming the entry (E); nothing of them is written. Each
// is the writer's first error, kept: a correct header, a write and the
// close after it give it at once and write nothing
namespace {
    // The first error kept: what the calls after it give, nothing written
    void expect_kept(tar::writer& w, const sgcl::io::buffer& sink, const compress::error& first) {
        size_t size = sink.size();
        auto h = w.write_header(file_entry("ok", 1));
        ASSERT_FALSE(h);
        EXPECT_TRUE(h.error() == first) << std::string(h.error().message().view());
        auto d = w.write(std::string("x"));
        ASSERT_FALSE(d);
        if (first.io_error()) {
            EXPECT_TRUE(d.error() == *first.io_error());
        } else {
            EXPECT_EQ(d.error().code(), make_error_code(first.code()));
        }
        auto c = w.close();
        ASSERT_FALSE(c);
        EXPECT_TRUE(c.error() == d.error());
        auto again = w.close();
        ASSERT_FALSE(again);
        EXPECT_TRUE(again.error() == d.error());
        EXPECT_EQ(sink.size(), size);
        ASSERT_TRUE(w.last_error());
        EXPECT_TRUE(*w.last_error() == first);
    }
}

TEST(Tar_Tests, WriterRefusesWhatItCannotWrite) {
    auto bad = [](const tar::entry& e) {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        auto r = w.write_header(e);
        if (r) {
            return compress::errc(0);
        }
        expect_kept(w, sink, r.error());
        EXPECT_EQ(sink.size(), 0u);
        return r.error().code();
    };
    EXPECT_EQ(bad(file_entry("", 0)), compress::errc::invalid_argument);
    EXPECT_EQ(bad(file_entry(std::string("a\0b", 3), 0)), compress::errc::invalid_argument);
    auto dir = file_entry("d/", 5);
    dir.type = tar::kind::directory;
    EXPECT_EQ(bad(dir), compress::errc::invalid_argument);
    auto dev = file_entry("dev", 0);
    dev.type = tar::kind::char_device;
    dev.dev_major = 2097152;
    EXPECT_EQ(bad(dev), compress::errc::invalid_argument);
    for (auto [k, v] : std::vector<std::pair<std::string, std::string>>{{"path", "x"}, {"size", "1"}, {"GNU.sparse.map", "0,0"}, {"a=b", "c"}, {"", "c"}, {std::string("k\0", 2), "v"}}) {
        auto e = file_entry("p", 0);
        e.pax.push_back({sgcl::string(k), sgcl::string(v)});
        EXPECT_EQ(bad(e), compress::errc::invalid_argument) << k;
    }
    auto twice = file_entry("p", 0);
    twice.pax.push_back({sgcl::string("comment"), sgcl::string("a")});
    twice.pax.push_back({sgcl::string("comment"), sgcl::string("b")});
    EXPECT_EQ(bad(twice), compress::errc::invalid_argument);
    auto huge = file_entry("p", 0);
    huge.pax.push_back({sgcl::string("comment"), sgcl::string(std::string(1 << 20, 'c'))});
    EXPECT_EQ(bad(huge), compress::errc::invalid_argument);

    // the data: no more than the size, and all of it before the next header or the close
    {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        auto none = w.write(std::string("x"));   // no entry yet
        ASSERT_FALSE(none);
        EXPECT_EQ(none.error().code(), make_error_code(compress::errc::invalid_argument));
        expect_kept(w, sink, *w.last_error());
        EXPECT_EQ(sink.size(), 0u);
    }
    {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        ASSERT_TRUE(w.write_header(file_entry("a/b.txt", 5)));
        size_t header = sink.size();
        auto past = w.write(std::string("123456"));
        ASSERT_FALSE(past);
        EXPECT_EQ(past.error().code(), make_error_code(compress::errc::invalid_argument));
        EXPECT_NE(std::string(past.error().message().view()).find("a/b.txt"), std::string::npos) << std::string(past.error().message().view());
        EXPECT_EQ(sink.size(), header);   // nothing of it written
        auto fits = w.write(std::string("12345"));   // what would fit gives the same error
        ASSERT_FALSE(fits);
        EXPECT_TRUE(fits.error() == past.error());
        expect_kept(w, sink, *w.last_error());
    }
    {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        ASSERT_TRUE(w.write_header(file_entry("a/b.txt", 5)));
        ASSERT_TRUE(w.write(std::string("123")));
        auto early = w.write_header(file_entry("c", 0));
        ASSERT_FALSE(early);
        EXPECT_EQ(early.error().code(), compress::errc::invalid_argument);
        EXPECT_NE(std::string(early.error().message().view()).find("entry a/b.txt"), std::string::npos) << std::string(early.error().message().view());
        auto rest = w.write(std::string("45"));
        ASSERT_FALSE(rest);
        EXPECT_EQ(rest.error().code(), make_error_code(compress::errc::invalid_argument));
        expect_kept(w, sink, early.error());
    }
    {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        ASSERT_TRUE(w.write_header(file_entry("a/b.txt", 5)));
        ASSERT_TRUE(w.write(std::string("123")));
        auto closing = w.close();
        ASSERT_FALSE(closing);
        EXPECT_EQ(closing.error().code(), make_error_code(compress::errc::invalid_argument));
        auto rest = w.write(std::string("45"));
        ASSERT_FALSE(rest);
        EXPECT_TRUE(rest.error() == closing.error());
        expect_kept(w, sink, *w.last_error());
    }
    {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        ASSERT_TRUE(w.write_header(file_entry("a/b.txt", 5)));
        ASSERT_TRUE(w.write(std::string("12345")));
        ASSERT_TRUE(w.close());
        ASSERT_TRUE(w.close());   // a second does nothing
        EXPECT_FALSE(w.last_error());
        auto after = w.write_header(file_entry("d", 0));
        ASSERT_FALSE(after);
        EXPECT_EQ(after.error().code(), compress::errc::io);
        EXPECT_TRUE(after.error().io_error()->is_closed());
        auto late = w.write(std::string("x"));
        ASSERT_FALSE(late);
        EXPECT_TRUE(late.error().is_closed());
        expect_kept(w, sink, after.error());
        // what was written is a whole archive
        auto archive = bytes_of(sink);
        EXPECT_EQ(archive.size(), 512u + 512u + 1024u);
        EXPECT_EQ(dump(archive), line_of(file_entry("a/b.txt", 5), "12345") + "\nEND\n");
    }
}

// A failing sink stops the writer for good, the error handed on as it was
TEST(Tar_Tests, AFailingSinkIsKept) {
    struct failing {
        int calls = 0;
        sgcl::expected<size_t, sgcl::io::error> write(sgcl::slice<const std::byte>) {
            ++calls;
            return sgcl::unexpected<sgcl::io::error>(sgcl::io::error(sgcl::error_code(EIO, std::system_category()), "write", "disk"));
        }
    } f;
    tar::writer w(f);
    auto h = w.write_header(file_entry("a", 1));
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code(), compress::errc::io);
    EXPECT_EQ(h.error().io_error()->code(), sgcl::error_code(EIO, std::system_category()));
    EXPECT_FALSE(w.write(std::string("x")));
    EXPECT_FALSE(w.close());
    EXPECT_EQ(f.calls, 1);
    ASSERT_TRUE(w.last_error());
    EXPECT_EQ(w.last_error()->io_error()->code(), sgcl::error_code(EIO, std::system_category()));
}

// The data of an entry is exactly its size (A6): read_all gives it, a
// read after gives 0, next() steps over what was not read, a read with
// no entry gives 0
TEST(Tar_Tests, AnEntrysDataIsItsSize) {
    auto entries = varied_entries();
    auto archive = write_all(entries);
    tar::reader r(dribble{archive, 1000});
    std::byte one[1];
    EXPECT_EQ(value_of(r.read(one)), 0u);   // before the first entry
    for (size_t i = 0; i < entries.size(); ++i) {
        auto e = r.next();
        ASSERT_TRUE(e && *e) << i;
        EXPECT_EQ((*e)->size, entries[i].second.size());
        if (i % 3 == 0) {
            continue;   // not read: next() steps over it
        }
        if (i % 3 == 1 && !entries[i].second.empty()) {
            ASSERT_EQ(value_of(r.read(one)), 1u);   // read in part
            continue;
        }
        auto all = r.read_all();
        ASSERT_TRUE(all);
        EXPECT_EQ(text(*all), entries[i].second) << i;
        EXPECT_EQ(value_of(r.read(one)), 0u);
    }
    auto end = r.next();
    ASSERT_TRUE(end);
    EXPECT_FALSE(*end);
    EXPECT_FALSE(value_of(r.next()));   // and again
    EXPECT_EQ(value_of(r.read(one)), 0u);
    EXPECT_FALSE(r.last_error());
    // copy_to, the mixin's: one entry
    tar::reader c(dribble{archive, 7});
    ASSERT_TRUE(c.next());
    sgcl::io::buffer sink;
    ASSERT_TRUE(c.copy_to(sink));
    EXPECT_EQ(bytes_of(sink), "hello world\n");
}

// The forms of the archive's end Go takes, and the ones it does not
TEST(Tar_Tests, TheEndOfAnArchive) {
    auto entry = file_entry("f", 600);
    std::string data(600, 'd');
    auto archive = write_all({{entry, data}});
    size_t body = 512 + 1024;   // header, data and padding
    auto line = line_of(entry, data) + "\n";
    EXPECT_EQ(dump(archive.substr(0, body)), line + "END\n");                        // no blocks of zeros
    EXPECT_EQ(dump(archive.substr(0, body + 512)), line + "END\n");                  // one
    EXPECT_EQ(dump(archive.substr(0, body - 100)), line + "END\n");                  // the padding cut: Go's end too
    EXPECT_EQ(dump(archive.substr(0, 512 + 300)), line_of(entry, data.substr(0, 300)) + "\nREADERR unexpected_end\n");
    EXPECT_EQ(dump(archive.substr(0, body + 100)), line + "ERR unexpected_end\n");    // a header cut
    EXPECT_EQ(dump(archive.substr(0, body + 512) + raw_header("g", '0', octal12(0))), line + "ERR invalid_header\n");
    EXPECT_EQ(dump(std::string()), "END\n");
    // the data cut, stepped over by next(): the error names the entry
    tar::reader r(dribble{archive.substr(0, 700), 64});
    ASSERT_TRUE(r.next());
    auto cut = r.next();
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), compress::errc::unexpected_end);
    EXPECT_NE(std::string(cut.error().message().view()).find("entry f:"), std::string::npos) << std::string(cut.error().message().view());
    EXPECT_EQ(cut.error().offset(), 700u);
    EXPECT_FALSE(r.next());   // it stays
    // the error of a read is the stream's, naming the entry
    tar::reader s(dribble{archive.substr(0, 700), 64});
    ASSERT_TRUE(s.next());
    auto all = s.read_all();
    ASSERT_FALSE(all);
    EXPECT_EQ(all.error().code(), make_error_code(compress::errc::unexpected_end));
    EXPECT_EQ(std::string(all.error().message().view()), "read tar entry f: unexpected end of data");
    EXPECT_EQ(s.last_error()->offset(), 700u);
}

// A header's checksum, taken unsigned and signed, and its numbers
TEST(Tar_Tests, HeaderChecksumsAndNumbers) {
    auto h = raw_header("x", '0', octal12(3));
    EXPECT_EQ(dump(h + padded("abc") + end_blocks).substr(0, 7), "x||0|3|");
    // a byte past 127 in the name: the signed sum (old Sun tar) is taken too
    auto sun = raw_header("\xe9", '0', octal12(0));
    int sum = 0;
    for (size_t i = 0; i < 512; ++i) {
        sum += i >= 148 && i < 156 ? ' ' : int(int8_t(sun[i]));
    }
    char f[8];
    std::snprintf(f, sizeof f, "%06o", sum);
    sun.replace(148, 7, std::string(f, 6) + '\0');
    EXPECT_EQ(dump(sun + end_blocks), "\\xe9||0|0|644|0|0|||0|-|-|0|0|0|00000000|\nEND\n");
    auto wrong = h;
    wrong[0] = 'y';
    EXPECT_EQ(dump(wrong + end_blocks), "ERR invalid_header\n");
    // the checksum's error at the second header: its offset
    auto two = write_all({{file_entry("a", 3), "abc"}});
    two = two.substr(0, 1024) + wrong + end_blocks;
    tar::reader r2(dribble{two, 9});
    ASSERT_TRUE(r2.next());
    auto e = r2.next();
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), compress::errc::invalid_header);
    EXPECT_EQ(e.error().offset(), 1024u);
    // base-256 (GNU): a size; negative and past 63 bits are not sizes
    auto b256 = raw_header("b", '0', std::string("\x80\0\0\0\0\0\0\0\0\0\0\x05", 12));
    EXPECT_EQ(dump(b256 + padded("12345") + end_blocks).substr(0, 7), "b||0|5|");
    auto neg_time = raw_header("t", '0', octal12(0));
    neg_time.replace(136, 12, std::string(11, '\xff') + '\x9c');   // -100 in two's complement
    neg_time.replace(148, 8, "        ");
    unsigned t_sum = 0;
    for (unsigned char c : neg_time) {
        t_sum += c;
    }
    std::snprintf(f, sizeof f, "%06o", t_sum);
    neg_time.replace(148, 7, std::string(f, 6) + '\0');
    EXPECT_EQ(dump(neg_time + end_blocks), "t||0|0|644|0|0|||-100000000000|-|-|0|0|0|00000000|\nEND\n");
    auto neg_size = raw_header("n", '0', std::string("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 12));
    EXPECT_EQ(dump(neg_size + end_blocks), "ERR invalid_header\n");
    auto over = raw_header("o", '0', std::string("\x80\x80\0\0\0\0\0\0\0\0\0\0", 12));
    EXPECT_EQ(dump(over + end_blocks), "ERR invalid_header\n");
    auto top = raw_header("o", '0', std::string("\x80\0\0\0\x80\0\0\0\0\0\0\0", 12));   // 2^63
    EXPECT_EQ(dump(top + end_blocks), "ERR invalid_header\n");
    auto letters = raw_header("l", '0', std::string("0000000008a\0", 12));
    EXPECT_EQ(dump(letters + end_blocks), "ERR invalid_header\n");
    auto spaces = raw_header("s", '0', std::string("  3 \0\0\0\0\0\0\0\0", 12));   // spaces and NULs around
    EXPECT_EQ(dump(spaces + padded("xyz") + end_blocks).substr(0, 7), "s||0|3|");
}

// The limits (B3): an extended header or a long name past 1 MiB is
// too_large before a byte of it is read; 1 MiB is read
TEST(Tar_Tests, TheLimitsOfExtendedHeaders) {
    const size_t mib = size_t(1) << 20;
    for (char flag : {'x', 'g', 'L', 'K'}) {
        EXPECT_EQ(dump(raw_header("h", flag, octal12(mib + 1))), "ERR too_large\n") << flag;   // nothing after it: not unexpected_end
    }
    std::string head = "1048576 comment=";
    std::string record = head + std::string(mib - head.size() - 1, 'c') + "\n";
    ASSERT_EQ(record.size(), mib);
    ASSERT_EQ(pax_record("comment", std::string(mib - head.size() - 1, 'c')), record);
    auto archive = raw_header("PaxHeaders.0/f", 'x', octal12(mib)) + record + raw_header("f", '0', octal12(0)) + end_blocks;
    tar::reader r(dribble{archive, 4096});
    auto e = r.next();
    ASSERT_TRUE(e && *e) << (e ? "" : std::string(e.error().message().view()));
    ASSERT_EQ((*e)->pax.size(), 1u);
    EXPECT_EQ((*e)->pax[0].second.size(), mib - head.size() - 1);
    std::string name(mib, 'n');
    auto gnu = raw_header("././@LongLink", 'L', octal12(mib)) + name + raw_header("short", '0', octal12(0)) + end_blocks;
    tar::reader g(dribble{gnu, 65536});
    auto ge = g.next();
    ASSERT_TRUE(ge && *ge);
    EXPECT_EQ((*ge)->name.size(), mib);
    // the global records altogether
    auto global = [&](const std::string& key) {
        auto rec = pax_record(key, std::string(600000, 'v'));
        return raw_header("g", 'g', octal12(rec.size())) + padded(rec);
    };
    EXPECT_EQ(dump(global("a") + raw_header("f", '0', octal12(0)) + end_blocks).substr(0, 2), "f|");
    EXPECT_EQ(dump(global("a") + global("b") + raw_header("f", '0', octal12(0)) + end_blocks), "ERR too_large\n");
    // records that are malformed, or values that are not what they stand for
    for (auto rec : {pax_record("size", "-1"), pax_record("size", "1x"), pax_record("uid", "u"), pax_record("mtime", "1.x"), pax_record("mtime", ".5"), std::string("6 =ab\n"), std::string("4 a=\n"),
                     std::string("9 abc\n"), std::string("99 a=b\n"), pax_record("path", std::string("a\0b", 3)), pax_record(std::string("k\0", 2), "v")}) {
        auto a = raw_header("x", 'x', octal12(rec.size())) + padded(rec) + raw_header("f", '0', octal12(0)) + end_blocks;
        EXPECT_EQ(dump(a), "ERR invalid_header\n") << rec;
    }
    // an empty value leaves the header's field
    auto keep = pax_record("uid", "") + pax_record("path", "");
    EXPECT_EQ(dump(raw_header("x", 'x', octal12(keep.size())) + padded(keep) + raw_header("f", '0', octal12(0)) + end_blocks).substr(0, 2), "f|");
}

// Sparse files are unsupported and stepped over; the error names the entry
TEST(Tar_Tests, SparseFilesAreSteppedOver) {
    auto archive = go_tar("sparse-formats.tar");
    tar::reader r(dribble{archive, 100});
    for (auto name : {"sparse-gnu", "sparse-posix-0.0", "sparse-posix-0.1", "sparse-posix-1.0"}) {
        auto e = r.next();
        ASSERT_FALSE(e) << name;
        EXPECT_EQ(e.error().code(), compress::errc::unsupported);
        EXPECT_NE(std::string(e.error().message().view()).find(std::string("entry ") + name + ":"), std::string::npos) << std::string(e.error().message().view());
        std::byte b[4];
        EXPECT_EQ(value_of(r.read(b)), 0u);   // no data of it
    }
    auto end = r.next();
    ASSERT_TRUE(end && *end);
    EXPECT_EQ((*end)->name, "end");
    EXPECT_EQ(text(value_of(r.read_all())), "end\n");
}

// Truncated at every byte and flipped at every bit: an error or entries,
// never a read out of bounds (ASan) or a hang
TEST(Tar_Tests, TruncatedAndCorruptArchivesFail) {
    Entries entries = {{file_entry("a.txt", 700), std::string(700, 'a')}};
    auto longer = file_entry(std::string(130, 'l'), 3);
    longer.pax.push_back({sgcl::string("comment"), sgcl::string("c")});
    entries.push_back({longer, "abc"});
    auto link = file_entry("s", 0);
    link.type = tar::kind::symlink;
    link.link_name = sgcl::string(std::string(120, 't'));
    entries.push_back({link, ""});
    auto archive = write_all(entries);
    auto whole = dump(archive);
    // the last entry's header ends here: a cut before it loses an entry
    size_t last = archive.size() - 1024;
    for (size_t n = 0; n < archive.size(); ++n) {
        auto d = dump(archive.substr(0, n), 1 + n % 5, 1 + n % 7);
        bool failed = d.find("ERR") != std::string::npos;
        EXPECT_TRUE(failed || d.ends_with("END\n")) << n;
        if (n < last) {
            EXPECT_NE(d, whole) << n;
        } else if (n == last || n == last + 512) {
            EXPECT_EQ(d, whole) << n;   // no zeros, or one block of them: Go's end
        } else {
            EXPECT_EQ(d.substr(d.rfind('\n', d.size() - 2) + 1), "ERR unexpected_end\n") << n;   // a block of zeros cut
        }
    }
    // the blocks of headers and records (not the data, not the zeros): a
    // bit of every byte, and every bit of the numeric fields and the
    // checksum of a header
    for (size_t block : {0, 3, 4, 5, 7, 8, 9}) {
        ASSERT_NE(archive.substr(block * 512, 512), std::string(512, '\0')) << block;
        for (size_t i = 0; i < 512; ++i) {
            for (int bit = 0; bit < 8; ++bit) {
                bool header = block == 0 || block == 3 || block == 5 || block == 7 || block == 9;
                if (bit != int(i % 8) && !(header && i >= 100 && i < 160)) {
                    continue;
                }
                auto flipped = archive;
                flipped[block * 512 + i] = char(flipped[block * 512 + i] ^ (1 << bit));
                (void)dump(flipped);
            }
        }
    }
}

TEST(Tar_Tests, IsLocal) {
    auto local = [](std::string name, tar::kind k = tar::kind::file, std::string link = "") {
        tar::entry e;
        e.name = sgcl::string(name);
        e.type = k;
        e.link_name = sgcl::string(link);
        return e.is_local();
    };
    // Go's filepath.IsLocal cases (Unix), and a backslash never local
    for (auto n : {"a", "a/b", "a/b/", "./a", ".", "a/../b", "a/./b", "a//b", "...", "..a", "a..", "a/b/../..", "a/../a/b"}) {
        EXPECT_TRUE(local(n)) << n;
    }
    for (auto n : {"", "/a", "/", "..", "../a", "a/../..", "a/../../b", "./..", "a/b/../../..", "a\\b", "..\\a"}) {
        EXPECT_FALSE(local(n)) << n;
    }
    EXPECT_TRUE(local("d/link", tar::kind::symlink, "target"));
    EXPECT_TRUE(local("d/link", tar::kind::symlink, "../target"));   // d/../target: inside
    EXPECT_FALSE(local("link", tar::kind::symlink, "../target"));
    EXPECT_FALSE(local("d/link", tar::kind::symlink, "../../target"));
    EXPECT_FALSE(local("d/link", tar::kind::symlink, "/etc/passwd"));
    EXPECT_FALSE(local("d/link", tar::kind::symlink, ""));
    EXPECT_TRUE(local("hard", tar::kind::hardlink, "d/file"));
    EXPECT_FALSE(local("d/hard", tar::kind::hardlink, "../file"));   // from the root: outside
    EXPECT_FALSE(local("../hard", tar::kind::hardlink, "file"));
}

// The task's forms: a writer and a reader on the scheduler
TEST(Tar_Tests, TheAsyncForms) {
    auto entries = varied_entries();
    auto want = expected_dump(entries);
    auto task = sgcl::async::spawn([](Entries entries) -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        tar::writer w(sink);
        for (auto& [e, data] : entries) {
            if (!co_await w.async_write_header(e)) {
                co_return "header failed";
            }
            if (!data.empty() && !co_await w.async_write(bytes(data))) {
                co_return "write failed";
            }
        }
        if (!co_await w.async_close()) {
            co_return "close failed";
        }
        std::string archive = bytes_of(sink);
        tar::reader r(dribble{archive, 777});
        std::string out;
        for (;;) {
            auto e = co_await r.async_next();
            if (!e) {
                co_return "next failed";
            }
            if (!*e) {
                break;
            }
            auto data = co_await r.async_read_all();
            if (!data) {
                co_return "read failed";
            }
            out += line_of(**e, text(*data)) + "\n";
        }
        co_return out + "END\n";
    }(entries));
    EXPECT_EQ(task.wait(), want);
    sgcl::async::scheduler::stop();
}

// A .tar.gz: the writer into gzip's writer, zlib inflating it; the reader
// over gzip's reader of what zlib made, and of what we made
TEST(Tar_Tests, InsideGzip) {
    auto entries = varied_entries();
    auto plain = write_all(entries);
    sgcl::io::buffer sink;
    {
        sgcl::tracked_ptr gz = make_tracked<compress::gzip::writer>(sink);
        tar::writer w(gz);
        for (auto& [e, data] : entries) {
            ASSERT_TRUE(w.write_header(e));
            ASSERT_TRUE(w.write(bytes(data)));
        }
        ASSERT_TRUE(w.close());
        ASSERT_TRUE(gz->close());
    }
    std::string back;
    ASSERT_TRUE(z_inflate(bytes_of(sink), 31, back));
    EXPECT_EQ(back, plain);
    auto zgz = z_deflate(plain, 6, 31);
    sgcl::tracked_ptr in = make_tracked<compress::gzip::reader>(dribble{zgz, 1000});
    tar::reader r(in);
    EXPECT_EQ(dump(r), expected_dump(entries));
    // and ours read back by ours
    sgcl::tracked_ptr ours = make_tracked<compress::gzip::reader>(dribble{bytes_of(sink), 777});
    tar::reader r2(ours);
    EXPECT_EQ(dump(r2), expected_dump(entries));
}

// A tar archive through flate both ways, as .tar.gz is made: its random
// entries make stored blocks between coded ones, which found a fault of
// the decoder's fast loop (the bytes its refill read ahead ORed into the
// header after a stored block)
TEST(Tar_Tests, FlateBothWaysOfATarArchive) {
    auto plain = write_all(varied_entries());
    for (int level : {1, 6, 9}) {
        auto c = text(compress::flate::compress(bytes(plain), {.level = level}));
        std::string z;
        EXPECT_TRUE(z_inflate(c, -15, z)) << level;
        EXPECT_EQ(z, plain) << level;
        auto d = compress::flate::decompress(bytes(c));
        ASSERT_TRUE(d) << "level " << level << ": " << std::string(d.error().message().view());
        EXPECT_EQ(text(*d), plain) << level;
    }
}

// close() closes the source, as buffered_reader's does
TEST(Tar_Tests, CloseClosesTheSource) {
    struct source {
        std::string data = std::string(1024, '\0');
        size_t at = 0;
        int closed = 0;
        sgcl::expected<size_t, sgcl::io::error> read(sgcl::slice<std::byte> b) {
            size_t n = std::min(b.size(), data.size() - at);
            std::memcpy(b.data(), data.data() + at, n);
            at += n;
            return n;
        }
        sgcl::expected<void, sgcl::io::error> close() {
            ++closed;
            return {};
        }
    } s;
    tar::reader r(s);
    EXPECT_FALSE(value_of(r.next()));
    ASSERT_TRUE(r.close());
    EXPECT_EQ(s.closed, 1);
}
