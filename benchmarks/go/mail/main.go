// The mail counterparts in Go: net/mail, mime, mime/multipart,
// mime/quotedprintable and net/smtp, one case per run
// (benchmarks/net/mail.cpp has the SGCL side, the same cases). Prints one
// line, ns per operation.
//
//	mime_build [n]   a message of a text of 2 KB and an HTML of 4 KB (both with non-ASCII,
//	                 quoted-printable), an attachment of 1 MB (base64 in lines of 76), a
//	                 subject in encoded words (mime.BEncoding) and addresses by mail.Address,
//	                 multipart/mixed over multipart/alternative written by multipart.Writer
//	                 into a bytes.Buffer: per message
//	mime_parse [n]   the same message read: mail.ReadMessage, mime.ParseMediaType,
//	                 multipart.Reader (which undoes quoted-printable), the text and HTML
//	                 read whole, the attachment decoded by base64.NewDecoder: per message
//	smtp_serve       a minimal SMTP server written by hand on bufio (EHLO, MAIL, RCPT,
//	                 DATA with its dots, RSET, NOOP, QUIT): prints "port N", serves until killed
//	smtp_client ADDR [n]  net/smtp's client sends messages of 4 KB over one session to ADDR:
//	                 per message
//	smtp_send ADDR [n]    the same client, as the one that feeds both servers (smtp_serve of
//	                 each side): per message
package main

import (
	"bufio"
	"bytes"
	"encoding/base64"
	"fmt"
	"io"
	"mime"
	"mime/multipart"
	"mime/quotedprintable"
	"net"
	"net/mail"
	"net/smtp"
	"net/textproto"
	"os"
	"strconv"
	"strings"
	"time"
)

func report(what string, el time.Duration, n int) {
	fmt.Printf("mail %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(el.Nanoseconds())/float64(n), float64(n)/el.Seconds(), el.Seconds())
}

func bodyText() string {
	var b strings.Builder
	for b.Len() < 2048 {
		b.WriteString("Zażółć gęślą jaźń: the quick brown fox jumps over the lazy dog, line after line.\r\n")
	}
	return b.String()
}

func bodyHTML() string {
	var b strings.Builder
	b.WriteString("<html><body>\r\n")
	for b.Len() < 4096 {
		b.WriteString("<p>Zażółć <b>gęślą</b> jaźń — the quick brown fox jumps over the lazy dog.</p>\r\n")
	}
	b.WriteString("</body></html>\r\n")
	return b.String()
}

func attachment() []byte {
	v := make([]byte, 1<<20)
	s := uint64(0x9E3779B97F4A7C15)
	for i := range v {
		s ^= s << 13
		s ^= s >> 7
		s ^= s << 17
		v[i] = byte(s)
	}
	return v
}

func build(text, html string, file []byte) []byte {
	var b bytes.Buffer
	b.Grow(len(file)*4/3 + 16384)
	from := mail.Address{Name: "Łucja Żółć", Address: "lucja@example.pl"}
	to := mail.Address{Name: "Bob Example", Address: "bob@example.com"}
	mixed := multipart.NewWriter(&b)
	fmt.Fprintf(&b, "Date: %s\r\nMessage-ID: <bench@example.pl>\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\nMIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=\"%s\"\r\n\r\n",
		time.Unix(1791203400, 0).UTC().Format(time.RFC1123Z), from.String(), to.String(), mime.BEncoding.Encode("utf-8", "Raport kwartalny — zażółć gęślą jaźń"), mixed.Boundary())
	var alt bytes.Buffer
	altw := multipart.NewWriter(&alt)
	h := textproto.MIMEHeader{}
	h.Set("Content-Type", "multipart/alternative; boundary=\""+altw.Boundary()+"\"")
	for _, p := range []struct{ t, s string }{{"text/plain; charset=utf-8", text}, {"text/html; charset=utf-8", html}} {
		ph := textproto.MIMEHeader{}
		ph.Set("Content-Type", p.t)
		ph.Set("Content-Transfer-Encoding", "quoted-printable")
		pw, _ := altw.CreatePart(ph)
		q := quotedprintable.NewWriter(pw)
		q.Write([]byte(p.s))
		q.Close()
	}
	altw.Close()
	pw, _ := mixed.CreatePart(h)
	pw.Write(alt.Bytes())
	ah := textproto.MIMEHeader{}
	ah.Set("Content-Type", "application/octet-stream")
	ah.Set("Content-Transfer-Encoding", "base64")
	ah.Set("Content-Disposition", mime.FormatMediaType("attachment", map[string]string{"filename": "raport.bin"}))
	aw, _ := mixed.CreatePart(ah)
	line := make([]byte, 78)
	for i := 0; i < len(file); i += 57 {
		j := i + 57
		if j > len(file) {
			j = len(file)
		}
		n := base64.StdEncoding.EncodedLen(j - i)
		base64.StdEncoding.Encode(line, file[i:j])
		line[n] = '\r'
		line[n+1] = '\n'
		aw.Write(line[:n+2])
	}
	mixed.Close()
	return b.Bytes()
}

func walk(h textproto.MIMEHeader, body io.Reader) int {
	mt, params, _ := mime.ParseMediaType(h.Get("Content-Type"))
	if strings.HasPrefix(mt, "multipart/") {
		total := 0
		r := multipart.NewReader(body, params["boundary"])
		for {
			p, err := r.NextPart()
			if err != nil {
				return total
			}
			total += walk(p.Header, p)
		}
	}
	if strings.EqualFold(h.Get("Content-Transfer-Encoding"), "base64") {
		b, _ := io.ReadAll(base64.NewDecoder(base64.StdEncoding, body))
		return len(b)
	}
	b, _ := io.ReadAll(body)
	return len(b)
}

func parse(wire []byte) int {
	m, err := mail.ReadMessage(bytes.NewReader(wire))
	if err != nil {
		panic(err)
	}
	dec := new(mime.WordDecoder)
	s, _ := dec.DecodeHeader(m.Header.Get("Subject"))
	list, _ := m.Header.AddressList("From")
	return walk(textproto.MIMEHeader(m.Header), m.Body) + len(s) + len(list)
}

func serve(c net.Conn) {
	defer c.Close()
	r := bufio.NewReaderSize(c, 8192)
	w := bufio.NewWriter(c)
	w.WriteString("220 go.bench ESMTP\r\n")
	w.Flush()
	for {
		line, err := r.ReadSlice('\n')
		if err != nil {
			return
		}
		verb := strings.ToUpper(string(bytes.Fields(line)[0]))
		switch verb {
		case "EHLO":
			w.WriteString("250-go.bench\r\n250 8BITMIME\r\n")
		case "HELO":
			w.WriteString("250 go.bench\r\n")
		case "MAIL":
			w.WriteString("250 2.1.0 ok\r\n")
		case "RCPT":
			w.WriteString("250 2.1.5 ok\r\n")
		case "DATA":
			w.WriteString("354 go on\r\n")
			w.Flush()
			var data bytes.Buffer
			for {
				l, err := r.ReadSlice('\n')
				if err != nil && err != bufio.ErrBufferFull {
					return
				}
				if len(l) == 3 && l[0] == '.' && l[1] == '\r' {
					break
				}
				if len(l) > 0 && l[0] == '.' {
					l = l[1:]
				}
				data.Write(l)
			}
			w.WriteString("250 2.0.0 ok\r\n")
		case "RSET", "NOOP":
			w.WriteString("250 2.0.0 ok\r\n")
		case "QUIT":
			w.WriteString("221 2.0.0 bye\r\n")
			w.Flush()
			return
		default:
			w.WriteString("500 5.5.1 what\r\n")
		}
		if r.Buffered() == 0 {
			w.Flush()
		}
	}
}

func client(what, addr string, n int) {
	msg := []byte("From: a@example.com\r\nTo: b@example.com\r\nSubject: bench\r\n\r\n" + strings.Repeat("x", 4096) + "\r\n")
	c, err := smtp.Dial(addr)
	if err != nil {
		panic(err)
	}
	if err := c.Hello("localhost"); err != nil {
		panic(err)
	}
	start := time.Now()
	for i := 0; i < n; i++ {
		if err := c.Mail("a@example.com"); err != nil {
			panic(err)
		}
		if err := c.Rcpt("b@example.com"); err != nil {
			panic(err)
		}
		w, err := c.Data()
		if err != nil {
			panic(err)
		}
		w.Write(msg)
		if err := w.Close(); err != nil {
			panic(err)
		}
	}
	report(what, time.Since(start), n)
	c.Quit()
}

func count(i int, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

func main() {
	switch os.Args[1] {
	case "mime_build":
		n := count(2, 2000)
		text, html, file := bodyText(), bodyHTML(), attachment()
		total := 0
		start := time.Now()
		for i := 0; i < n; i++ {
			total += len(build(text, html, file))
		}
		report("mime_build", time.Since(start), n)
		if total < n<<20 {
			os.Exit(1)
		}
	case "mime_parse":
		n := count(2, 2000)
		wire := build(bodyText(), bodyHTML(), attachment())
		total := 0
		start := time.Now()
		for i := 0; i < n; i++ {
			total += parse(wire)
		}
		report("mime_parse", time.Since(start), n)
		if total < n<<20 {
			os.Exit(1)
		}
	case "smtp_serve":
		l, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			panic(err)
		}
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		for {
			c, err := l.Accept()
			if err != nil {
				return
			}
			go serve(c)
		}
	case "smtp_client", "smtp_send":
		client(os.Args[1], os.Args[2], count(3, 20000))
	}
}
