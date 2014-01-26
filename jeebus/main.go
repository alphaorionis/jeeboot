package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"strconv"
	"strings"

	"github.com/jcw/jeebus"
)

const CONFIG_FILE = "./config.json"   // boot server configuration file
const FIRMWARE_PREFIX = "./firmware/" // location of hex files to serve

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
	rdClient.Register("RF12demo/"+dev, &JeeBootService{dev, loadConfig()})

	var msg jeebus.Message
	msg.Set("text", "8b 212g 31i 1c")
	msg.Publish("if/RF12demo/" + dev)

	// TODO need a mechanism to wait for client disconnect, possibly w/ retries
	<-make(chan byte) // wait forever
}

type Config struct {
	// map 16-byte hardware ID to the assigned pairing info
	HwIds map[string]struct {
		Type, Group, NodeId float64
	}
	// names for each of the node types
	Types map[string]string
	// name nodetype-group-nodeid to a map of swId's to filenames
	Nodes map[string]map[string]string
}

func loadConfig() (config Config) {
	data, err := ioutil.ReadFile(CONFIG_FILE)
	check(err)
	err = json.Unmarshal(data, &config)
	check(err)
	log.Printf("CONFIG %+v", config)
	return
}

type JeeBootService struct {
	dev    string
	config Config
}

func (s *JeeBootService) Handle(m *jeebus.Message) {
	text := m.Get("text")
	// log.Print(text)
	if strings.HasPrefix(text, "OK ") {
		var buf bytes.Buffer
		// convert the line of decimal byte values to a byte buffer
		for _, v := range strings.Split(text[3:], " ") {
			n, err := strconv.Atoi(v)
			check(err)
			buf.WriteByte(byte(n))
		}
		s.RespondToRequest(buf.Bytes())
	}
}

type PairingRequest struct {
	Header  uint8
	Variant uint8     // variant of remote node, 1..250 freely available
	Board   uint8     // type of remote node, 100..250 freely available
	Group   uint8     // current network group, 1..250 or 0 if unpaired
	NodeId  uint8     // current node ID, 1..30 or 0 if unpaired
	Check   uint16    // crc checksum over the current shared key
	HwId    [16]uint8 // unique hardware ID or 0's if not available
}

type PairingReply struct {
	Variant uint8     // variant of remote node, 1..250 freely available
	Board   uint8     // type of remote node, 100..250 freely available
	Group   uint8     // assigned network group, 1..250
	NodeId  uint8     // assigned node ID, 1..30
	ShKey   [16]uint8 // shared key or 0's if not used
}

type UpgradeRequest struct {
	Header  uint8
	Variant uint8  // variant of remote node, 1..250 freely available
	Board   uint8  // type of remote node, 100..250 freely available
	SwId    uint16 // current software ID 0 if unknown
	SwSize  uint16 // current software download size, in units of 16 bytes
	SwCheck uint16 // current crc checksum over entire download
}

type UpgradeReply struct {
	Variant uint8  // variant of remote node, 1..250 freely available
	Board   uint8  // type of remote node, 100..250 freely available
	SwId    uint16 // assigned software ID
	SwSize  uint16 // software download size, in units of 16 bytes
	SwCheck uint16 // crc checksum over entire download
}

type DownloadRequest struct {
	Header  uint8
	SwId    uint16 // current software ID
	SwIndex uint16 // current download index, as multiple of payload size
}

type DownloadReply struct {
	SwIdXor uint16    // current software ID xor current download index
	Data    [64]uint8 // download payload
}

func (s *JeeBootService) RespondToRequest(req []byte) {
	// fmt.Printf("%s %X %d\n", s.dev, req, len(req))
	reader := bytes.NewReader(req)
	switch len(req) {
	case 23:
		var preq PairingRequest
		err := binary.Read(reader, binary.LittleEndian, &preq)
		fmt.Printf("%+v\n", &preq)
		check(err)
	case 9:
		var ureq UpgradeRequest
		err := binary.Read(reader, binary.LittleEndian, &ureq)
		fmt.Printf("%+v\n", &ureq)
		check(err)
	case 5:
		var dreq DownloadRequest
		err := binary.Read(reader, binary.LittleEndian, &dreq)
		fmt.Printf("%+v\n", &dreq)
		check(err)
	default:
		log.Printf("bad req? %db = %X", len(req), req)
	}
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
