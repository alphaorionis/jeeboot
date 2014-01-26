package main

import (
	"bytes"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"

	"github.com/jcw/jeebus"
)

func main() {
	if len(os.Args) <= 1 {
		log.Fatalf("usage: jeeboot <cmd> ...")
	}

	switch os.Args[1] {

	case "boots":
		boots(os.Args[2]) // expect "usb-..." as 2nd arg

	default:
		log.Fatal("unknown sub-command: jeeboot ", os.Args[1], " ...")
	}
}

func boots(dev string) {
	rdClient := jeebus.NewClient("rd")
	rdClient.Register("RF12demo/#", &JeeBootService{dev})

	var msg jeebus.Message
	msg.Set("text", "8b 212g 31i 1c")
	msg.Publish("if/RF12demo/" + dev)

	<-make(chan byte) // wait forever
}

type JeeBootService struct {
	dev string
}

func (s *JeeBootService) Handle(m *jeebus.Message) {
	dev := strings.SplitN(m.T, "/", 2)[1]
	if dev != s.dev {
		return // not the device used for JeeBoot, ignore it
	}
	text := m.Get("text")
	log.Print(text)
	if strings.HasPrefix(text, "OK ") {
		var buf bytes.Buffer
		// convert the line of decimal byte values to a byte buffer
		for _, v := range strings.Split(text[3:], " ") {
			n, err := strconv.Atoi(v)
			check(err)
			buf.WriteByte(byte(n))
		}
		now := m.GetInt64("time")
		dev := strings.SplitN(m.T, "/", 2)[1]
		fmt.Printf("%d %s %X\n", now, dev, buf.Bytes())
	}
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
