// The Go oracle of the encoding::email tests: net/mail, mime, mime/multipart
// and mime/quotedprintable of the standard library.
//
//	go_mail summary FILE...   one JSON line per message: the subject (decoded
//	                          by mime.WordDecoder), the addresses
//	                          (Header.AddressList), the date (Header.Date),
//	                          the first text/plain and text/html of the body
//	                          and the attachments (multipart.Reader, which
//	                          undoes quoted-printable; base64 undone here)
//	go_mail build DIR         messages written with multipart.Writer,
//	                          quotedprintable.Writer, mime.QEncoding and
//	                          mime.FormatMediaType, and DIR/summary.jsonl
//	go_mail qp-encode / qp-decode   stdin to stdout through quotedprintable
package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"mime"
	"mime/multipart"
	"mime/quotedprintable"
	"net/mail"
	"net/textproto"
	"os"
	"path/filepath"
	"strings"
	"time"
)

func fnv(b []byte) string {
	h := uint64(0xcbf29ce484222325)
	for _, c := range b {
		h ^= uint64(c)
		h *= 0x100000001b3
	}
	return fmt.Sprintf("%016x", h)
}

type summaryT struct {
	Subject     string     `json:"subject"`
	From        [][]string `json:"from"`
	To          [][]string `json:"to"`
	Cc          [][]string `json:"cc"`
	Date        int64      `json:"date"`
	MessageID   string     `json:"message_id"`
	Text        string     `json:"text"`
	HTML        string     `json:"html"`
	Attachments [][]any    `json:"attachments"`
}

func addrs(h mail.Header, name string) [][]string {
	out := [][]string{}
	list, err := h.AddressList(name)
	if err != nil {
		return out
	}
	for _, a := range list {
		out = append(out, []string{a.Name, a.Address})
	}
	return out
}

func decodeBody(cte string, r io.Reader) []byte {
	b, _ := io.ReadAll(r)
	switch strings.ToLower(strings.TrimSpace(cte)) {
	case "base64":
		clean := strings.Map(func(r rune) rune {
			if r == '\r' || r == '\n' || r == ' ' || r == '\t' {
				return -1
			}
			return r
		}, string(b))
		d, _ := base64.StdEncoding.DecodeString(clean)
		return d
	case "quoted-printable":
		d, _ := io.ReadAll(quotedprintable.NewReader(bytes.NewReader(b)))
		return d
	}
	return b
}

func walk(h textproto.MIMEHeader, body io.Reader, s *summaryT, top bool) {
	ct := h.Get("Content-Type")
	if ct == "" {
		ct = "text/plain"
	}
	mt, params, err := mime.ParseMediaType(ct)
	if err != nil {
		mt = "text/plain"
	}
	if strings.HasPrefix(mt, "multipart/") {
		r := multipart.NewReader(body, params["boundary"])
		for {
			p, err := r.NextPart()
			if err != nil {
				return
			}
			walk(p.Header, p, s, false)
		}
	}
	disp, dparams, _ := mime.ParseMediaType(h.Get("Content-Disposition"))
	filename := dparams["filename"]
	if filename == "" {
		filename = params["name"]
	}
	data := decodeBody(h.Get("Content-Transfer-Encoding"), body)
	if disp != "attachment" && filename == "" {
		if mt == "text/plain" && s.Text == "" {
			s.Text = strings.ReplaceAll(string(data), "\r\n", "\n")
			return
		}
		if mt == "text/html" && s.HTML == "" {
			s.HTML = strings.ReplaceAll(string(data), "\r\n", "\n")
			return
		}
	}
	if !top && (disp == "attachment" || filename != "") {
		s.Attachments = append(s.Attachments, []any{filename, mt, len(data), fnv(data)})
	}
}

func summarize(path string) summaryT {
	f, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	defer f.Close()
	m, err := mail.ReadMessage(f)
	if err != nil {
		panic(err)
	}
	s := summaryT{From: [][]string{}, To: [][]string{}, Cc: [][]string{}, Attachments: [][]any{}}
	dec := new(mime.WordDecoder)
	s.Subject, _ = dec.DecodeHeader(m.Header.Get("Subject"))
	s.From = addrs(m.Header, "From")
	s.To = addrs(m.Header, "To")
	s.Cc = addrs(m.Header, "Cc")
	if d, err := m.Header.Date(); err == nil {
		s.Date = d.Unix()
	}
	s.MessageID = strings.TrimSpace(m.Header.Get("Message-ID"))
	walk(textproto.MIMEHeader(m.Header), m.Body, &s, true)
	return s
}

func write(dir string, n int, msg []byte) {
	if err := os.WriteFile(filepath.Join(dir, fmt.Sprintf("%02d.eml", n)), msg, 0o644); err != nil {
		panic(err)
	}
}

func build(dir string) {
	os.MkdirAll(dir, 0o755)
	date := time.Date(2026, 10, 5, 12, 30, 0, 0, time.UTC).Format(time.RFC1123Z)

	// 0: text in quoted-printable, a subject in Q words, addresses by mail.Address
	{
		var b bytes.Buffer
		from := mail.Address{Name: "Łucja Żółć", Address: "lucja@example.pl"}
		to := mail.Address{Name: "Doe, John", Address: "john@example.com"}
		fmt.Fprintf(&b, "From: %s\r\nTo: %s\r\nSubject: %s\r\nDate: %s\r\nMessage-ID: <go0@example.com>\r\n", from.String(), to.String(),
			mime.QEncoding.Encode("utf-8", "Zażółć gęślą jaźń"), date)
		fmt.Fprintf(&b, "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n")
		w := quotedprintable.NewWriter(&b)
		w.Write([]byte("Treść = zażółć gęślą jaźń, a line long enough to be broken by the encoder at seventy-six characters.\r\nSecond.\r\n"))
		w.Close()
		write(dir, 0, b.Bytes())
	}
	// 1: multipart/mixed with alternative and attachments, file names by FormatMediaType
	{
		var b bytes.Buffer
		mw := multipart.NewWriter(&b)
		fmt.Fprintf(&b, "From: a@example.com\r\nTo: b@example.com, \"C D\" <c@example.com>\r\nSubject: %s\r\nDate: %s\r\nMIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=%s\r\n\r\n",
			mime.BEncoding.Encode("utf-8", "Załączniki — attachments"), date, mw.Boundary())
		var alt bytes.Buffer
		aw := multipart.NewWriter(&alt)
		h := textproto.MIMEHeader{}
		h.Set("Content-Type", "multipart/alternative; boundary="+aw.Boundary())
		p, _ := mw.CreatePart(h)
		th := textproto.MIMEHeader{}
		th.Set("Content-Type", "text/plain; charset=utf-8")
		th.Set("Content-Transfer-Encoding", "quoted-printable")
		tp, _ := aw.CreatePart(th)
		qw := quotedprintable.NewWriter(tp)
		qw.Write([]byte("Plain text, ąę.\r\n"))
		qw.Close()
		hh := textproto.MIMEHeader{}
		hh.Set("Content-Type", "text/html; charset=utf-8")
		hp, _ := aw.CreatePart(hh)
		hp.Write([]byte("<p>HTML</p>\r\n"))
		aw.Close()
		p.Write(alt.Bytes())
		data := bytes.Repeat([]byte{0, 1, 2, 250, 251, 252}, 3000)
		ah := textproto.MIMEHeader{}
		ah.Set("Content-Type", "application/octet-stream")
		ah.Set("Content-Transfer-Encoding", "base64")
		ah.Set("Content-Disposition", mime.FormatMediaType("attachment", map[string]string{"filename": "dane ż.bin"}))
		ap, _ := mw.CreatePart(ah)
		enc := base64.StdEncoding.EncodeToString(data)
		for len(enc) > 76 {
			ap.Write([]byte(enc[:76] + "\r\n"))
			enc = enc[76:]
		}
		ap.Write([]byte(enc + "\r\n"))
		mw.Close()
		write(dir, 1, b.Bytes())
	}
	// 2: an attachment named in Content-Type's name only, LF line endings
	{
		msg := "From: x@example.com\nTo: y@example.com\nSubject: =?ISO-8859-1?Q?Caf=E9?=\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"b1\"\n\n" +
			"--b1\nContent-Type: text/plain\n\nbody\n--b1\nContent-Type: application/pdf; name=\"doc.pdf\"\nContent-Transfer-Encoding: base64\n\n" +
			base64.StdEncoding.EncodeToString([]byte("%PDF fake")) + "\n--b1--\n"
		write(dir, 2, []byte(msg))
	}
	f, _ := os.Create(filepath.Join(dir, "summary.jsonl"))
	defer f.Close()
	for i := 0; i < 3; i++ {
		s := summarize(filepath.Join(dir, fmt.Sprintf("%02d.eml", i)))
		j, _ := json.Marshal(s)
		f.Write(append(j, '\n'))
	}
}

func main() {
	switch os.Args[1] {
	case "summary":
		for _, p := range os.Args[2:] {
			j, _ := json.Marshal(summarize(p))
			fmt.Println(string(j))
		}
	case "build":
		build(os.Args[2])
	case "qp-encode":
		w := quotedprintable.NewWriter(os.Stdout)
		io.Copy(w, os.Stdin)
		w.Close()
	case "qp-decode":
		b, err := io.ReadAll(quotedprintable.NewReader(os.Stdin))
		os.Stdout.Write(b)
		if err != nil {
			os.Exit(1)
		}
	}
}
