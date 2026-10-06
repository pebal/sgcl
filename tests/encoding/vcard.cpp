//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// vcard: RFC 6350's vCard 4.0, RFC 2426's 3.0 read. The examples of both RFCs,
// groups, bare TYPE parameters, address books, the writer (VERSION first,
// folding), errors and their places, new versions and the boundaries.
#include "common.h"

#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    const char* rfc6350 = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Simon Perreault\r\nN:Perreault;Simon;;;ing. jr,M.Sc.\r\nBDAY:--0203\r\n"
                          "ANNIVERSARY:20090808T1430-0500\r\nGENDER:M\r\nLANG;PREF=1:fr\r\nLANG;PREF=2:en\r\nORG;TYPE=work:Viagenie\r\n"
                          "ADR;TYPE=work:;Suite D2-630;2875 Laurier;\r\n Quebec;QC;G1V 2M2;Canada\r\n"
                          "TEL;VALUE=uri;TYPE=\"work,voice\";PREF=1:tel:+1-418-656-9254;ext=102\r\n"
                          "TEL;VALUE=uri;TYPE=\"work,cell,voice,video,text\":tel:+1-418-262-6501\r\n"
                          "EMAIL;TYPE=work:simon.perreault@viagenie.ca\r\nGEO;TYPE=work:geo:46.772673,-71.282945\r\n"
                          "KEY;TYPE=work;VALUE=uri:\r\n http://www.viagenie.ca/simon.perreault/simon.asc\r\nTZ:-0500\r\n"
                          "URL;TYPE=home:http://nomis80.org\r\nEND:VCARD\r\n";

    const char* rfc2426 = "BEGIN:vCard\r\nVERSION:3.0\r\nFN:Frank Dawson\r\nORG:Lotus Development Corporation\r\n"
                          "ADR;TYPE=WORK,POSTAL,PARCEL:;;6544 Battleford Drive\r\n ;Raleigh;NC;27613-3502;U.S.A.\r\n"
                          "TEL;TYPE=VOICE,MSG,WORK:+1-919-676-9515\r\nTEL;TYPE=FAX,WORK:+1-919-676-9564\r\n"
                          "EMAIL;TYPE=INTERNET,PREF:Frank_Dawson@Lotus.com\r\nEMAIL;TYPE=INTERNET:fdawson@earthlink.net\r\n"
                          "URL:http://home.earthlink.net/~fdawson\r\nEND:vCard\r\n";
}

TEST(Vcard_Tests, Rfc6350) {
    auto r = vcard::parse(rfc6350);
    ASSERT_TRUE(r) << r.error().message();
    vcard c = *r;
    EXPECT_EQ(c.version(), "4.0");
    EXPECT_EQ(c.formatted_name(), "Simon Perreault");
    auto n = c.property_of("N")->components();
    ASSERT_EQ(n.size(), 5u);
    EXPECT_EQ(n[0], "Perreault");
    EXPECT_EQ(n[4], "ing. jr,M.Sc.");
    auto adr = c.property_of("adr")->components();
    ASSERT_EQ(adr.size(), 7u);
    EXPECT_EQ(adr[2], "2875 Laurier");
    EXPECT_EQ(adr[3], "Quebec");
    auto tels = c.properties_of("TEL");
    ASSERT_EQ(tels.size(), 2u);
    EXPECT_EQ(tels[0].param("TYPE"), "work,voice");   // quoted: one value
    EXPECT_EQ(tels[0].param("PREF"), "1");
    EXPECT_EQ(tels[0].value(), "tel:+1-418-656-9254;ext=102");
    EXPECT_EQ(c.properties_of("LANG").size(), 2u);
    EXPECT_EQ(c.text("KEY", "?"), "http://www.viagenie.ca/simon.perreault/simon.asc");
    EXPECT_EQ(c.property_of("TZ")->as_utc_offset(), sgcl::duration(std::chrono::hours(-5)));
    EXPECT_EQ(c.text("NICKNAME", "none"), "none");
    auto back = vcard::parse(c.to_string());
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, c);
    EXPECT_EQ(back->to_string(), c.to_string());
}

TEST(Vcard_Tests, Rfc2426) {
    auto c = vcard::parse(rfc2426);
    ASSERT_TRUE(c) << c.error().message();
    EXPECT_EQ(c->version(), "3.0");
    EXPECT_EQ(c->formatted_name(), "Frank Dawson");
    auto emails = c->properties_of("EMAIL");
    ASSERT_EQ(emails.size(), 2u);
    ASSERT_EQ(emails[0].params()[0].values.size(), 2u);   // unquoted: a list
    EXPECT_EQ(emails[0].params()[0].values[1], "PREF");
    EXPECT_EQ(c->property_of("ADR")->components()[3], "Raleigh");
    // 2.1's and 3.0's bare types, groups
    auto old = vcard::parse("BEGIN:VCARD\nVERSION:3.0\nFN:A\nTEL;HOME;VOICE:123\nitem1.EMAIL;type=INTERNET:a@b.c\nitem1.X-ABLabel:Private\nEND:VCARD\n");
    ASSERT_TRUE(old) << old.error().message();
    auto tel = old->property_of("TEL");
    ASSERT_EQ(tel->params().size(), 2u);
    EXPECT_EQ(tel->params()[1].name, "TYPE");
    EXPECT_EQ(tel->params()[1].values[0], "VOICE");
    EXPECT_EQ(old->property_of("EMAIL")->group(), "item1");
    EXPECT_EQ(old->property_of("X-ABLABEL")->text(), "Private");
    EXPECT_EQ(*vcard::parse(old->to_string()), *old);
}

TEST(Vcard_Tests, AddressBooks) {
    std::string book = std::string(rfc6350) + rfc2426 + "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Third\r\nEND:VCARD\r\n";
    auto all = vcard::parse_all(sgcl::string(book));
    ASSERT_TRUE(all);
    ASSERT_EQ(all->size(), 3u);
    EXPECT_EQ((*all)[2].formatted_name(), "Third");
    EXPECT_EQ(vcard::parse(sgcl::string(book)).error().code(), errc::syntax);
    EXPECT_EQ(vcard::parse_all("")->size(), 0u);
    EXPECT_EQ(vcard::parse("").error().code(), errc::syntax);
}

TEST(Vcard_Tests, Writing) {
    vcard c = vcard()
                  .add(content_line::text("FN", "Jan Kowalski"))
                  .add(content_line("N", "Kowalski;Jan;;;"))
                  .add(content_line("EMAIL", {{"TYPE", {"work", "pref"}}}, "jan@example.com"))
                  .add(content_line::text("NOTE", "line one\nline two; with, punctuation").with_group("item1"));
    EXPECT_EQ(std::string(c.to_string().view()),
              "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Jan Kowalski\r\nN:Kowalski;Jan;;;\r\nEMAIL;TYPE=work,pref:jan@example.com\r\n"
              "item1.NOTE:line one\\nline two\\; with\\, punctuation\r\nEND:VCARD\r\n");
    EXPECT_EQ(c.property_of("NOTE")->text(), "line one\nline two; with, punctuation");
    // VERSION first whatever the order held
    vcard late = vcard().erase("VERSION").add(content_line("FN", "x")).add(content_line("VERSION", "4.0"));
    EXPECT_EQ(std::string(late.to_string().view()), "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:x\r\nEND:VCARD\r\n");
    EXPECT_EQ(c.set(content_line("FN", "Other")).formatted_name(), "Other");
    EXPECT_EQ(c.set(content_line("FN", "Other")).properties().size(), c.properties().size());
    EXPECT_EQ(c.erase("email").properties_of("EMAIL").size(), 0u);
    EXPECT_EQ(c.erase("none"), c);
    EXPECT_NE(c, c.erase("FN"));
    EXPECT_EQ(vcard().erase("VERSION").version(), "");
    EXPECT_THROW(vcard().add(content_line("A B", "x")).to_string(), sgcl::invalid_argument);
}

TEST(Vcard_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"BEGIN:VCARD\r\nFN:x\r\n", errc::unexpected_end, 3, 1},
        {"BEGIN:VCARD\r\nBEGIN:VCARD\r\nEND:VCARD\r\nEND:VCARD\r\n", errc::depth_limit, 2, 1},
        {"BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n", errc::syntax, 1, 1},
        {"BEGIN:VCARD\r\nFN\r\nEND:VCARD\r\n", errc::syntax, 2, 3},
        {"BEGIN:VCARD\r\n.FN:x\r\nEND:VCARD\r\n", errc::syntax, 2, 1},
        {"BEGIN:VCARD\r\nEND:VCALENDAR\r\n", errc::mismatched_tag, 2, 1},
    };
    for (auto& c : cases) {
        auto r = vcard::parse(c.text);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text;
        EXPECT_EQ(r.error().column(), c.column) << c.text;
    }
    vcard::options o;
    o.max_size = 8;
    EXPECT_EQ(vcard::parse(rfc2426, o).error().code(), errc::limit_exceeded);
}

TEST(Vcard_Tests, Boundaries) {
    vcard none;
    EXPECT_EQ(none.version(), "4.0");
    EXPECT_EQ(none.formatted_name(), "");
    EXPECT_EQ(none.properties().size(), 1u);
    EXPECT_EQ(*vcard::parse(none.to_string()), none);
    vcard a = none;
    vcard b = std::move(a);
    EXPECT_EQ(b, none);
    std::string text = rfc6350;
    for (size_t n = 0; n <= text.size(); n += 3) {
        (void)vcard::parse(sgcl::string(text.substr(0, n)));
    }
    for (size_t piece : {1, 7, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(text, piece));
        auto r = vcard::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ(r->formatted_name(), "Simon Perreault");
    }
    sgcl::io::reader bad(make_tracked<failing>("BEGIN:VCARD"));
    EXPECT_EQ(vcard::parse(bad).error().code(), errc::io);
    auto task = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 5));
        auto r = co_await vcard::async_parse(in);
        co_return r && r->version() == "4.0" ? 1 : -1;
    }(text));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}
