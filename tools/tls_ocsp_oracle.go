// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the OCSP stapling tests (tests/net/tls_revocation.cpp): a
// crypto/tls server of Go's standard library, as an oracle only, that
// staples the OCSP response of -ocsp (tls.Certificate.OCSPStaple) to the
// chain of -cert (the leaf first) and -key. It listens on 127.0.0.1 at a
// port of the system's choice, prints "LISTEN <port>", takes -n connections
// one after another (1 by default), prints "STATE version=<hex>" after each
// handshake (Go staples when the client asked with status_request), answers
// every line with the line
// reversed, and prints "CLOSE_NOTIFY" or "ERROR <text>" at its end. -tls12
// keeps it to TLS 1.2 (CertificateStatus), else 1.3 alone (the status in the
// leaf's CertificateEntry). With -client it is a client instead: it dials
// -client's address, verifies against -ca for -name, asks for a staple, and
// prints "STAPLE <bytes>" (0 when none came) and "GOT <line>". Built by the
// test with `go build`.
package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
)

func main() {
	cert := flag.String("cert", "", "the chain, PEM, the leaf first")
	key := flag.String("key", "", "the key, PEM")
	ocsp := flag.String("ocsp", "", "the OCSP response to staple, DER")
	n := flag.Int("n", 1, "connections")
	v12 := flag.Bool("tls12", false, "TLS 1.2 alone")
	client := flag.String("client", "", "dial this address instead")
	ca := flag.String("ca", "", "the client's roots, PEM")
	name := flag.String("name", "localhost", "the client's server name")
	flag.Parse()
	if *client != "" {
		dial(*client, *ca, *name)
		return
	}
	pair, err := tls.LoadX509KeyPair(*cert, *key)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	if *ocsp != "" {
		staple, err := os.ReadFile(*ocsp)
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		pair.OCSPStaple = staple
	}
	cfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13}
	if *v12 {
		cfg.MinVersion = tls.VersionTLS12
		cfg.MaxVersion = tls.VersionTLS12
	}
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	fmt.Println("LISTEN", l.Addr().(*net.TCPAddr).Port)
	for i := 0; i < *n; i++ {
		c, err := l.Accept()
		if err != nil {
			fmt.Println("ERROR", err)
			return
		}
		serve(tls.Server(c, cfg))
	}
}

func serve(c *tls.Conn) {
	defer c.Close()
	if err := c.Handshake(); err != nil {
		fmt.Println("ERROR", err)
		return
	}
	fmt.Printf("STATE version=%x\n", c.ConnectionState().Version)
	r := bufio.NewReader(c)
	for {
		line, err := r.ReadString('\n')
		if err != nil {
			if errors.Is(err, io.EOF) {
				fmt.Println("CLOSE_NOTIFY")
			} else {
				fmt.Println("ERROR", err)
			}
			return
		}
		line = strings.TrimSuffix(line, "\n")
		b := []byte(line)
		for i, j := 0, len(b)-1; i < j; i, j = i+1, j-1 {
			b[i], b[j] = b[j], b[i]
		}
		c.Write(append(b, '\n'))
	}
}

// The client: Go's crypto/tls asks for a staple (status_request) always and
// reports what came in ConnectionState.OCSPResponse
func dial(addr, ca, name string) {
	pem, err := os.ReadFile(ca)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	roots := x509.NewCertPool()
	roots.AppendCertsFromPEM(pem)
	c, err := tls.Dial("tcp", addr, &tls.Config{RootCAs: roots, ServerName: name})
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	defer c.Close()
	fmt.Println("STAPLE", len(c.ConnectionState().OCSPResponse))
	c.Write([]byte("hello go\n"))
	line, err := bufio.NewReader(c).ReadString('\n')
	if err != nil {
		fmt.Println("ERROR", err)
		return
	}
	fmt.Println("GOT", strings.TrimSuffix(line, "\n"))
}
