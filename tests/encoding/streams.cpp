//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The codecs as streams: the encoders as writers, the decoders as readers,
// the dumper. The class of bug a stream always meets is the edge of a
// block, so everything here is fed and read in pieces of 1, 2, 3 and 7
// bytes as well as whole, and must come out as the whole-text codec says.
#include "common.h"

using namespace sgcl::encoding;

#include <string>

using namespace enc_test;

namespace {
    const size_t Pieces[] = {1, 2, 3, 7, 4096};

    // What a stream encoder writes for `in` written in pieces of `piece`
    template<class Make>
    std::string encoded_in_pieces(Make make, const std::vector<byte>& in, size_t piece) {
        sgcl::tracked_ptr out = make_tracked<sink>();
        auto enc = make(out);
        for (size_t at = 0; at < in.size(); at += piece) {
            size_t k = std::min(piece, in.size() - at);
            auto w = enc->write(slice<const byte>(in.data() + at, k));
            EXPECT_TRUE(w.has_value());
            EXPECT_EQ(*w, k);
        }
        EXPECT_TRUE(enc->close().has_value());
        EXPECT_TRUE(enc->close().has_value());   // a second close does nothing
        return out->text;
    }

    template<class Make>
    sgcl::expected<std::string, io::error> decoded_in_pieces(Make make, const std::string& text, size_t piece, size_t buffer) {
        auto dec = make(make_tracked<dribble>(text, piece));
        return read_all(*dec, buffer);
    }
}

TEST(CodecStreams_Tests, EncodersWrittenInPieces) {
    for (size_t n : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 29, 64, 100, 300, 10000, 30000}) {
        auto in = input(n);
        for (size_t piece : Pieces) {
            EXPECT_EQ(encoded_in_pieces([](auto out) { return base64::standard.encoder_to(out); }, in, piece), base64::standard.encode(as_slice(in)).view()) << n << " by " << piece;
            EXPECT_EQ(encoded_in_pieces([](auto out) { return base64::raw_url.encoder_to(out); }, in, piece), base64::raw_url.encode(as_slice(in)).view()) << n << " by " << piece;
            EXPECT_EQ(encoded_in_pieces([](auto out) { return base32::standard.encoder_to(out); }, in, piece), base32::standard.encode(as_slice(in)).view()) << n << " by " << piece;
            EXPECT_EQ(encoded_in_pieces([](auto out) { return base32::hex.without_padding().encoder_to(out); }, in, piece), base32::hex.without_padding().encode(as_slice(in)).view()) << n << " by " << piece;
            EXPECT_EQ(encoded_in_pieces([](auto out) { return ascii85::encoder_to(out); }, in, piece), ascii85::encode(as_slice(in)).view()) << n << " by " << piece;
        }
    }
}

TEST(CodecStreams_Tests, DecodersReadInPieces) {
    for (size_t n : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 29, 64, 100, 300, 10000, 30000}) {
        auto in = input(n);
        auto want = std::string(view_of(in));
        auto t64 = std::string(base64::standard.encode(as_slice(in)).view());
        auto t64r = std::string(base64::raw_url.encode(as_slice(in)).view());
        auto t32 = std::string(base32::standard.encode(as_slice(in)).view());
        auto t32r = std::string(base32::hex.without_padding().encode(as_slice(in)).view());
        auto t85 = std::string(ascii85::encode(as_slice(in)).view());
        for (size_t piece : Pieces) {
            for (size_t buffer : Pieces) {
                auto where = std::to_string(n) + " fed by " + std::to_string(piece) + " read by " + std::to_string(buffer);
                EXPECT_EQ(decoded_in_pieces([](auto r) { return base64::standard.decoder_from(std::move(r)); }, t64, piece, buffer).value(), want) << where;
                EXPECT_EQ(decoded_in_pieces([](auto r) { return base64::raw_url.decoder_from(std::move(r)); }, t64r, piece, buffer).value(), want) << where;
                EXPECT_EQ(decoded_in_pieces([](auto r) { return base32::standard.decoder_from(std::move(r)); }, t32, piece, buffer).value(), want) << where;
                EXPECT_EQ(decoded_in_pieces([](auto r) { return base32::hex.without_padding().decoder_from(std::move(r)); }, t32r, piece, buffer).value(), want) << where;
                EXPECT_EQ(decoded_in_pieces([](auto r) { return ascii85::decoder_from(std::move(r)); }, t85, piece, buffer).value(), want) << where;
            }
        }
    }
}

// A lenient decoder skips line endings wherever a piece of the stream
// cuts them, as MIME wraps its lines at 76
TEST(CodecStreams_Tests, LenientDecoderAcrossLines) {
    auto in = input(1000);
    auto text = std::string(base64::standard.encode(as_slice(in)).view());
    std::string wrapped;
    for (size_t i = 0; i < text.size(); i += 76) {
        wrapped += text.substr(i, 76) + "\r\n";
    }
    for (size_t piece : Pieces) {
        for (size_t buffer : {size_t(1), size_t(5), size_t(4096)}) {
            auto r = decoded_in_pieces([](auto r) { return base64::standard.lenient().decoder_from(std::move(r)); }, wrapped, piece, buffer);
            ASSERT_TRUE(r.has_value());
            EXPECT_EQ(*r, view_of(in));
        }
    }
    // a strict decoder refuses the first line ending
    auto dec = base64::standard.decoder_from(make_tracked<dribble>(wrapped, 7));
    auto r = read_all(*dec, 100);
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(dec->last_error().has_value());
    EXPECT_EQ(dec->last_error()->offset(), 76u);
    EXPECT_EQ(dec->last_error()->code(), encoding::errc::invalid_character);
}

// An invalid text fails the read that reaches it, after the bytes before
// it were handed out, and every read after; last_error() says where, the
// same offset the whole-text decoding names
TEST(CodecStreams_Tests, ErrorsOfAStreamAreTheErrorsOfTheText) {
    const char* bad[] = {"QUJDRA==QUJD", "QUJ*", "QQ=x", "QR==", "QUJDR", "QQ", "QQ=", "QUJD=", "QUJ\nD"};
    for (auto text : bad) {
        auto whole = base64::standard.decode(text);
        ASSERT_FALSE(whole.has_value()) << text;
        for (size_t piece : Pieces) {
            for (size_t buffer : Pieces) {
                auto dec = base64::standard.decoder_from(make_tracked<dribble>(text, piece));
                std::string got;
                std::vector<byte> buf(buffer);
                expected<size_t, io::error> r;
                do {
                    r = dec->read(slice<byte>(buf.data(), buf.size()));
                    if (r) {
                        got.append(reinterpret_cast<const char*>(buf.data()), *r);
                    }
                } while (r && *r);
                ASSERT_FALSE(r.has_value()) << text << " by " << piece;
                EXPECT_EQ(r.error().code(), make_error_code(whole.error().code())) << text;
                EXPECT_EQ(r.error().code().category().name(), std::string_view("encoding"));
                EXPECT_EQ(std::string_view(r.error().message()).substr(0, 14), "decode base64:");
                ASSERT_TRUE(dec->last_error().has_value());
                EXPECT_EQ(dec->last_error()->offset(), whole.error().offset()) << text << " by " << piece << "/" << buffer;
                EXPECT_EQ(dec->last_error()->code(), whole.error().code()) << text;
                // the bytes before the error came out
                auto good = base64::standard.lenient().decode(string(std::string(text).substr(0, whole.error().offset() / 4 * 4)));
                if (good) {
                    EXPECT_EQ(got.substr(0, good->size()), view_of(bytes_of(*good))) << text;
                }
                // and the error stays
                auto again = dec->read(slice<byte>(buf.data(), buf.size()));
                EXPECT_FALSE(again.has_value());
            }
        }
    }
    // the reader under it failing is that reader's error, and last_error
    // carries it with the offset reached
    auto dec = base64::standard.decoder_from(make_tracked<failing>("QUJD"));
    auto r = read_all(*dec, 100);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().path(), "failing");
    ASSERT_TRUE(dec->last_error().has_value());
    EXPECT_EQ(dec->last_error()->code(), encoding::errc::io);
    EXPECT_EQ(dec->last_error()->offset(), 4u);
    ASSERT_TRUE(dec->last_error()->io_error().has_value());
    EXPECT_EQ(dec->last_error()->io_error()->path(), "failing");
}

TEST(CodecStreams_Tests, EncoderAfterCloseAndOverABrokenWriter) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    auto enc = base64::standard.encoder_to(out);
    ASSERT_TRUE(enc->write("f").has_value());
    EXPECT_EQ(out->text, "");            // one byte waits for its group
    ASSERT_TRUE(enc->close().has_value());
    EXPECT_EQ(out->text, "Zg==");
    EXPECT_TRUE(enc->is_closed());
    auto after = enc->write("x");
    ASSERT_FALSE(after.has_value());
    EXPECT_TRUE(after.error().is_closed());

    auto bad = base64::standard.encoder_to(make_tracked<broken>());
    auto w = bad->write("abc");
    ASSERT_FALSE(w.has_value());
    EXPECT_EQ(w.error().path(), "broken");
    auto bad2 = base64::standard.encoder_to(make_tracked<broken>());
    ASSERT_TRUE(bad2->write("ab").has_value());   // carried, nothing written yet
    EXPECT_FALSE(bad2->close().has_value());
}

// A writer underneath that fails once has lost the group being written:
// the failure is kept for good, as Go's encoder keeps it, and every later
// write and close reports it — nothing goes on as if the bytes were there
TEST(CodecStreams_Tests, AFailureOfTheWriterUnderneathIsKept) {
    auto check = [](auto make, const char* what) {
        sgcl::tracked_ptr out = make_tracked<flaky>();
        auto enc = make(out);
        ASSERT_TRUE(enc->write(as_slice(input(1))).has_value()) << what;   // carried
        out->fail_next = 1;
        auto w = enc->write(as_slice(input(40)));
        ASSERT_FALSE(w.has_value()) << what;
        EXPECT_EQ(w.error().path(), "flaky") << what;
        auto again = enc->write(as_slice(input(40)));   // the writer works again; the encoder does not pretend
        ASSERT_FALSE(again.has_value()) << what;
        EXPECT_EQ(again.error().path(), "flaky") << what;
        auto c = enc->close();
        ASSERT_FALSE(c.has_value()) << what;
        EXPECT_EQ(c.error().path(), "flaky") << what;
        EXPECT_TRUE(enc->is_closed()) << what;
        EXPECT_FALSE(enc->close().has_value()) << what;
        EXPECT_EQ(out->text, "") << what;
    };
    check([](auto out) { return base64::standard.encoder_to(out); }, "base64");
    check([](auto out) { return base32::hex.encoder_to(out); }, "base32");
    check([](auto out) { return ascii85::encoder_to(out); }, "ascii85");
    check([](auto out) { return hex::dumper_to(out); }, "dumper");

    // the failure at close: kept too
    sgcl::tracked_ptr out = make_tracked<flaky>();
    auto enc = base64::standard.encoder_to(out);
    ASSERT_TRUE(enc->write("ab").has_value());
    out->fail_next = 1;
    EXPECT_FALSE(enc->close().has_value());
    EXPECT_FALSE(enc->close().has_value());
    EXPECT_FALSE(enc->write("x").has_value());
}

// The async forms on the paths of failure: the same errors as the
// blocking ones
TEST(CodecStreams_Tests, AsyncFailures) {
    auto t = sgcl::async::spawn([]() -> async::task<int> {
        // the writer underneath failing, kept through async_write and async_close
        sgcl::tracked_ptr out = make_tracked<flaky>();
        auto enc = base64::standard.encoder_to(out);
        co_await enc->async_write("a");
        out->fail_next = 1;
        if (co_await enc->async_write("bcdef")) {
            co_return -1;
        }
        if (co_await enc->async_write("bcdef")) {
            co_return -2;
        }
        if (enc->close()) {
            co_return -3;
        }
        sgcl::tracked_ptr dumped = make_tracked<flaky>();
        auto d = hex::dumper_to(dumped);
        co_await d->async_write("0123456789");
        dumped->fail_next = 1;
        if (d->close()) {
            co_return -4;
        }
        if (co_await d->async_write("x")) {
            co_return -5;
        }
        // a text that goes wrong, read in a task
        auto dec = base64::standard.decoder_from(make_tracked<dribble>("QUJDRA==QUJD", 3));
        std::vector<byte> buf(2);
        std::string got;
        expected<size_t, io::error> r;
        for (;;) {
            r = co_await dec->async_read(slice<byte>(buf.data(), buf.size()));
            if (!r || *r == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf.data()), *r);
        }
        if (r || got != "ABCD" || !dec->last_error() || dec->last_error()->offset() != 8) {
            co_return -6;
        }
        if (co_await dec->async_read(slice<byte>(buf.data(), buf.size()))) {
            co_return -7;
        }
        // the reader underneath failing
        auto broken_in = ascii85::decoder_from(make_tracked<failing>("87cUR"));
        auto e = co_await broken_in->async_read(slice<byte>(buf.data(), buf.size()));
        while (e && *e) {
            e = co_await broken_in->async_read(slice<byte>(buf.data(), buf.size()));
        }
        if (e || broken_in->last_error()->code() != encoding::errc::io) {
            co_return -8;
        }
        // varints: past 64 bits and cut short
        sgcl::tracked_ptr big = make_tracked<io::buffered_reader>(make_tracked<dribble>(std::string(9, '\xFF') + "\x02", 1));
        auto v = co_await varint::async_read(*big);
        if (v || v.error().code() != make_error_code(encoding::errc::out_of_range)) {
            co_return -9;
        }
        sgcl::tracked_ptr cut = make_tracked<io::buffered_reader>(make_tracked<dribble>("\x80\x80", 1));
        auto u = co_await varint::async_read_signed(*cut);
        if (u || !u.error().is_eof()) {
            co_return -10;
        }
        sgcl::tracked_ptr failed = make_tracked<io::buffered_reader>(make_tracked<failing>("\x80"));
        auto f = co_await varint::async_read(*failed);
        if (f || f.error().path() != "failing") {
            co_return -11;
        }
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

TEST(CodecStreams_Tests, DumperInPieces) {
    for (size_t n : {0, 1, 15, 16, 17, 31, 32, 33, 100, 2000}) {
        auto in = input(n);
        for (size_t piece : Pieces) {
            sgcl::tracked_ptr out = make_tracked<sink>();
            auto d = hex::dumper_to(out);
            for (size_t at = 0; at < in.size(); at += piece) {
                size_t k = std::min(piece, in.size() - at);
                ASSERT_TRUE(d->write(slice<const byte>(in.data() + at, k)).has_value());
            }
            ASSERT_TRUE(d->close().has_value());
            EXPECT_EQ(out->text, hex::dump(as_slice(in)).view()) << n << " by " << piece;
        }
    }
}

TEST(CodecStreams_Tests, AsyncForms) {
    auto t = sgcl::async::spawn([]() -> async::task<int> {
        auto in = input(5000);
        sgcl::tracked_ptr out = make_tracked<sink>();
        auto enc = base64::standard.encoder_to(out);
        for (size_t at = 0; at < in.size(); at += 7) {
            auto w = co_await enc->async_write(slice<const byte>(in.data() + at, std::min<size_t>(7, in.size() - at)));
            if (!w) {
                co_return -1;
            }
        }
        if (!(enc->close())) {
            co_return -2;
        }
        if (out->text != base64::standard.encode(as_slice(in)).view()) {
            co_return -3;
        }
        auto dec = base64::standard.decoder_from(make_tracked<dribble>(out->text, 3));
        std::string got;
        std::vector<byte> buf(5);
        for (;;) {
            auto r = co_await dec->async_read(slice<byte>(buf.data(), buf.size()));
            if (!r) {
                co_return -4;
            }
            if (*r == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf.data()), *r);
        }
        if (got != view_of(in)) {
            co_return -5;
        }
        sgcl::tracked_ptr dumped = make_tracked<sink>();
        auto d = hex::dumper_to(dumped);
        co_await d->async_write(slice<const byte>(in.data(), 40));
        d->close();
        if (dumped->text != hex::dump(slice<const byte>(in.data(), 40)).view()) {
            co_return -6;
        }
        auto a85 = ascii85::encoder_to(dumped);
        co_await a85->async_write(slice<const byte>(in.data(), 3));
        a85->close();
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// The stream codecs hold their block through a tracked_ptr and the reader
// under them through another: a decoder left to itself while a collection
// runs keeps both
TEST(CodecStreams_Tests, SurvivesACollection) {
    auto in = input(20000);
    auto text = std::string(base64::standard.encode(as_slice(in)).view());
    auto dec = base64::standard.decoder_from(make_tracked<dribble>(text, 1000));
    std::string got;
    std::vector<byte> buf(333);
    for (;;) {
        sgcl::collector::force_collect(true);
        auto r = dec->read(slice<byte>(buf.data(), buf.size()));
        ASSERT_TRUE(r.has_value());
        if (*r == 0) {
            break;
        }
        got.append(reinterpret_cast<const char*>(buf.data()), *r);
    }
    EXPECT_EQ(got, view_of(in));
}

// hex has the forms base64 has: the sizes, the caller's buffer, the streams;
// ascii85 the buffer forms, its sizes bounds
TEST(CodecStreams_Tests, HexAndAscii85HaveTheWholeFamily) {
    auto data = sgcl::string("hello, codec");
    auto bytes = as_bytes(data.as_slice());
    EXPECT_EQ(hex::encoded_size(bytes.size()), 24u);
    sgcl::vector<char> text(hex::encoded_size(bytes.size()));
    size_t n = hex::encode_to(text.as_slice(), bytes);
    EXPECT_EQ(sgcl::string(text.data(), n), hex::encode(bytes));
    sgcl::vector<byte> back(hex::max_decoded_size(n));
    auto m = hex::decode_to(back.as_slice(), sgcl::string(text.data(), n));
    ASSERT_TRUE(m);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(back.data()), *m), "hello, codec");

    tracked_ptr<io::buffer> sink = make_tracked<io::buffer>();
    auto enc = hex::encoder_to(sink);
    ASSERT_TRUE(enc->write(bytes));
    ASSERT_TRUE(enc->close());
    EXPECT_EQ(sink->text(), hex::encode(bytes));
    auto dec = hex::decoder_from(make_tracked<io::buffer>(sink->text()));
    auto all = io::read_all(dec);
    ASSERT_TRUE(all);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(all->data()), all->size()), "hello, codec");

    sgcl::vector<char> a85(ascii85::max_encoded_size(bytes.size()));
    size_t k = ascii85::encode_to(a85.as_slice(), bytes);
    EXPECT_EQ(sgcl::string(a85.data(), k), ascii85::encode(bytes));
    sgcl::vector<byte> a85back(ascii85::max_decoded_size(k));
    auto j = ascii85::decode_to(a85back.as_slice(), sgcl::string(a85.data(), k));
    ASSERT_TRUE(j);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(a85back.data()), *j), "hello, codec");
}
