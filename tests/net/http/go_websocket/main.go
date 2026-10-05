// A WebSocket peer in Go for the interop tests, written by hand from RFC
// 6455 with the standard library alone (net/http for the server's upgrade,
// crypto/sha1 and encoding/base64 for the handshake, compress/flate for
// permessage-deflate without context takeover; no gorilla, no x/net):
//
//	go_websocket server [deflate]   an echo server at /echo: a message comes back as it
//	                                came; "frag:N" is answered by "fragmented" in N frames
//	                                with a ping between two of them, "close:CODE" by a close
//	                                frame of that code and the reason "bye"; prints "port N"
//	go_websocket client URL         a client of URL: a text, bytes, a text in three frames
//	                                with a ping among them, a message of 100 000 bytes, then
//	                                a close of 1000; prints what came back, a line each
//
// The server ends at a request for /quit, or after two minutes.
package main

import (
	"bufio"
	"bytes"
	"compress/flate"
	"crypto/rand"
	"crypto/sha1"
	"encoding/base64"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"
)

const guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

func accept(key string) string {
	h := sha1.Sum([]byte(key + guid))
	return base64.StdEncoding.EncodeToString(h[:])
}

type conn struct {
	c       net.Conn
	r       *bufio.Reader
	client  bool // masks what it sends
	deflate bool // permessage-deflate, no context takeover either way
}

func (w *conn) writeFrame(fin bool, rsv1 bool, op byte, payload []byte) error {
	var h []byte
	b0 := op
	if fin {
		b0 |= 0x80
	}
	if rsv1 {
		b0 |= 0x40
	}
	h = append(h, b0)
	mask := byte(0)
	if w.client {
		mask = 0x80
	}
	n := len(payload)
	switch {
	case n < 126:
		h = append(h, mask|byte(n))
	case n <= 0xFFFF:
		h = append(h, mask|126, byte(n>>8), byte(n))
	default:
		h = append(h, mask|127)
		h = binary.BigEndian.AppendUint64(h, uint64(n))
	}
	data := payload
	if w.client {
		var key [4]byte
		rand.Read(key[:])
		h = append(h, key[:]...)
		data = make([]byte, n)
		for i := range payload {
			data[i] = payload[i] ^ key[i%4]
		}
	}
	_, err := w.c.Write(append(h, data...))
	return err
}

func (w *conn) readFrame() (fin bool, rsv1 bool, op byte, payload []byte, err error) {
	var h [2]byte
	if _, err = io.ReadFull(w.r, h[:]); err != nil {
		return
	}
	fin = h[0]&0x80 != 0
	rsv1 = h[0]&0x40 != 0
	op = h[0] & 0x0F
	masked := h[1]&0x80 != 0
	if masked == w.client {
		err = errors.New("mask on the wrong side")
		return
	}
	n := uint64(h[1] & 0x7F)
	switch n {
	case 126:
		var b [2]byte
		if _, err = io.ReadFull(w.r, b[:]); err != nil {
			return
		}
		n = uint64(binary.BigEndian.Uint16(b[:]))
	case 127:
		var b [8]byte
		if _, err = io.ReadFull(w.r, b[:]); err != nil {
			return
		}
		n = binary.BigEndian.Uint64(b[:])
	}
	var key [4]byte
	if masked {
		if _, err = io.ReadFull(w.r, key[:]); err != nil {
			return
		}
	}
	payload = make([]byte, n)
	if _, err = io.ReadFull(w.r, payload); err != nil {
		return
	}
	if masked {
		for i := range payload {
			payload[i] ^= key[i%4]
		}
	}
	return
}

func compress(p []byte) []byte {
	var b bytes.Buffer
	fw, _ := flate.NewWriter(&b, flate.BestSpeed)
	fw.Write(p)
	fw.Flush()
	out := b.Bytes()
	return out[:len(out)-4] // the sync flush's 00 00 FF FF
}

func decompress(p []byte) ([]byte, error) {
	r := flate.NewReader(io.MultiReader(bytes.NewReader(p), bytes.NewReader([]byte{0, 0, 0xFF, 0xFF, 1, 0, 0, 0xFF, 0xFF})))
	return io.ReadAll(r)
}

// A message: its frames put together, control frames answered on the
// way; op 8 for a close (its payload)
func (w *conn) readMessage() (op byte, data []byte, err error) {
	var compressed bool
	first := true
	for {
		fin, rsv1, fop, payload, e := w.readFrame()
		if e != nil {
			return 0, nil, e
		}
		switch fop {
		case 9:
			w.writeFrame(true, false, 10, payload)
			continue
		case 10:
			continue
		case 8:
			return 8, payload, nil
		}
		if first {
			op = fop
			compressed = rsv1
			first = false
		}
		data = append(data, payload...)
		if fin {
			break
		}
	}
	if compressed {
		data, err = decompress(data)
	}
	return
}

func (w *conn) writeMessage(op byte, data []byte) error {
	if w.deflate {
		return w.writeFrame(true, true, op, compress(data))
	}
	return w.writeFrame(true, false, op, data)
}

// --- the server ---------------------------------------------------------------

func serve(deflate bool) {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	go func() {
		time.Sleep(2 * time.Minute)
		os.Exit(0)
	}()
	mux := http.NewServeMux()
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) { os.Exit(0) })
	mux.HandleFunc("/echo", func(w http.ResponseWriter, r *http.Request) {
		if !strings.EqualFold(r.Header.Get("Upgrade"), "websocket") || r.Header.Get("Sec-WebSocket-Version") != "13" {
			http.Error(w, "not a websocket", http.StatusBadRequest)
			return
		}
		key := r.Header.Get("Sec-WebSocket-Key")
		c, rw, err := w.(http.Hijacker).Hijack()
		if err != nil {
			return
		}
		defer c.Close()
		head := "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept(key) + "\r\n"
		for _, p := range strings.Split(r.Header.Get("Sec-WebSocket-Protocol"), ",") {
			if strings.TrimSpace(p) == "chat" {
				head += "Sec-WebSocket-Protocol: chat\r\n"
				break
			}
		}
		useDeflate := deflate && strings.Contains(r.Header.Get("Sec-WebSocket-Extensions"), "permessage-deflate")
		if useDeflate {
			head += "Sec-WebSocket-Extensions: permessage-deflate; server_no_context_takeover; client_no_context_takeover\r\n"
		}
		c.Write([]byte(head + "\r\n"))
		ws := &conn{c: c, r: rw.Reader, deflate: useDeflate}
		for {
			op, data, err := ws.readMessage()
			if err != nil {
				return
			}
			if op == 8 {
				ws.writeFrame(true, false, 8, data)
				return
			}
			text := string(data)
			switch {
			case op == 1 && strings.HasPrefix(text, "frag:"):
				n, _ := strconv.Atoi(text[5:])
				msg := []byte("fragmented")
				size := (len(msg) + n - 1) / n
				for i := 0; i < n; i++ {
					end := min((i+1)*size, len(msg))
					part := msg[min(i*size, len(msg)):end]
					fop := byte(0)
					if i == 0 {
						fop = 1
					}
					ws.writeFrame(i == n-1, false, fop, part)
					if i == 0 {
						ws.writeFrame(true, false, 9, []byte("ping in between"))
					}
				}
			case op == 1 && strings.HasPrefix(text, "close:"):
				code, _ := strconv.Atoi(text[6:])
				ws.writeFrame(true, false, 8, append(binary.BigEndian.AppendUint16(nil, uint16(code)), "bye"...))
				ws.readMessage()
				return
			default:
				ws.writeMessage(op, data)
			}
		}
	})
	(&http.Server{Handler: mux}).Serve(l)
}

// --- the client ---------------------------------------------------------------

func client(raw string) {
	u, err := url.Parse(raw)
	if err != nil {
		fmt.Println("error:", err)
		return
	}
	c, err := net.Dial("tcp", u.Host)
	if err != nil {
		fmt.Println("error:", err)
		return
	}
	defer c.Close()
	var k [16]byte
	rand.Read(k[:])
	key := base64.StdEncoding.EncodeToString(k[:])
	fmt.Fprintf(c, "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: other, chat\r\n\r\n", u.RequestURI(), u.Host, key)
	r := bufio.NewReader(c)
	res, err := http.ReadResponse(r, nil)
	if err != nil {
		fmt.Println("error:", err)
		return
	}
	if res.StatusCode != 101 || res.Header.Get("Sec-WebSocket-Accept") != accept(key) {
		fmt.Println("handshake:", res.StatusCode)
		return
	}
	fmt.Println("protocol:", res.Header.Get("Sec-WebSocket-Protocol"))
	ws := &conn{c: c, r: r, client: true}
	ws.writeMessage(1, []byte("hello"))
	ws.writeMessage(2, []byte{1, 2, 3})
	ws.writeFrame(false, false, 1, []byte("in "))
	ws.writeFrame(true, false, 9, []byte("ping"))
	ws.writeFrame(false, false, 0, []byte("three "))
	ws.writeFrame(true, false, 0, []byte("frames"))
	big := bytes.Repeat([]byte("0123456789"), 10000)
	ws.writeMessage(2, big)
	for i := 0; i < 4; i++ {
		op, data, err := ws.readMessage()
		if err != nil {
			fmt.Println("error:", err)
			return
		}
		if op == 1 {
			fmt.Printf("text: %s\n", data)
		} else {
			fmt.Printf("binary: %d same=%v\n", len(data), len(data) < 10 || bytes.Equal(data, big))
		}
	}
	ws.writeFrame(true, false, 8, append(binary.BigEndian.AppendUint16(nil, 1000), "done"...))
	op, data, err := ws.readMessage()
	if err != nil || op != 8 || len(data) < 2 {
		fmt.Println("close: none")
		return
	}
	fmt.Printf("close: %d\n", binary.BigEndian.Uint16(data))
}

func main() {
	switch {
	case len(os.Args) >= 2 && os.Args[1] == "server":
		serve(len(os.Args) >= 3 && os.Args[2] == "deflate")
	case len(os.Args) >= 3 && os.Args[1] == "client":
		client(os.Args[2])
	default:
		fmt.Fprintln(os.Stderr, "usage: go_websocket server [deflate] | client URL")
		os.Exit(2)
	}
}
