package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"encoding/hex"
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
	config := loadConfig()
	fw := loadAllFirmware(config)

	rdClient := jeebus.NewClient("rd")
	rdClient.Register("RF12demo/"+dev, &JeeBootService{dev, config, fw})

	msg := map[string]interface{}{"text": "8b 212g 31i 1c"}
	jeebus.Publish("if/RF12demo/"+dev, msg)

	// TODO need a mechanism to wait for client disconnect, possibly w/ retries
	<-make(chan byte) // wait forever
}

type Config struct {
	// map 16-byte hardware ID to the assigned pairing info
	Pairs map[string]struct {
		Board, Group, Node float64
	}
	// swId's for each of the pairs
	Nodes map[string]map[string]float64
	// map each swId to a filename
	Files map[string]string
}

func loadConfig() (config Config) {
	data, err := ioutil.ReadFile(CONFIG_FILE)
	check(err)
	err = json.Unmarshal(data, &config)
	check(err)
	log.Printf("CONFIG %+v", config)
	return
}

type Firmware struct {
	name string
	crc  uint16
	data []byte
}

func loadAllFirmware(config Config) map[int]Firmware {
	fw := make(map[int]Firmware)
	for key, name := range config.Files {
		swId, err := strconv.Atoi(key)
		check(err)
		fw[swId] = readFirmware(name)
	}
	return fw
}

func readFirmware(name string) Firmware {
	buf := readIntelHexFile(name)
	data := padToBinaryMultiple(buf, 64)
	log.Printf("data %d -> %d bytes", buf.Len(), len(data))

	return Firmware{name, calculateCrc(data), data}
}

func readIntelHexFile(name string) bytes.Buffer {
	fd, err := os.Open(FIRMWARE_PREFIX + name)
	check(err)
	scanner := bufio.NewScanner(fd)
	var buf bytes.Buffer
	for scanner.Scan() {
		t := scanner.Text()
		if strings.HasPrefix(t, ":") {
			b, err := hex.DecodeString(t[1:])
			check(err)
			// TODO probably doesn't handle hex files over 64 KB
			if b[3] == 0 {
				buf.Write(b[4 : 4+b[0]])
			}
		}
	}
	return buf
}

func padToBinaryMultiple(buf bytes.Buffer, count int) []byte {
	for buf.Len()%count != 0 {
		buf.WriteByte(0xFF)
	}
	return buf.Bytes()
}

var crcTable = []uint16{
	0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
	0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
}

func calculateCrc(buf []byte) uint16 {
	var crc uint16 = 0xFFFF
	for _, data := range buf {
		crc = crc>>4 ^ crcTable[crc&0x0F] ^ crcTable[data&0x0F]
		crc = crc>>4 ^ crcTable[crc&0x0F] ^ crcTable[data>>4]
	}
	return crc
}

type JeeBootService struct {
	dev    string
	config Config
	fw     map[int]Firmware
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
		s.respondToRequest(buf.Bytes())
	}
}

type PairingRequest struct {
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
	SwId    uint16 // current software ID
	SwIndex uint16 // current download index, as multiple of payload size
}

type DownloadReply struct {
	SwIdXor uint16    // current software ID xor current download index
	Data    [64]uint8 // download payload
}

func (s *JeeBootService) respondToRequest(req []byte) {
	// fmt.Printf("%s %X %d\n", s.dev, req, len(req))
	switch len(req) - 1 {
	case 22:
		var preq PairingRequest
		header := s.unpackReq(req, &preq)
		fmt.Printf("%08b %+v\n", header, preq)
	case 8:
		var ureq UpgradeRequest
		header := s.unpackReq(req, &ureq)
		fmt.Printf("%08b %+v\n", header, ureq)
	case 4:
		var dreq DownloadRequest
		header := s.unpackReq(req, &dreq)
		fmt.Printf("%08b %+v\n", header, dreq)
	default:
		log.Printf("bad req? %db = %X", len(req), req)
	}
}

func (s *JeeBootService) unpackReq(req []byte, out interface{}) (h uint8) {
	reader := bytes.NewReader(req)
	err := binary.Read(reader, binary.LittleEndian, &h)
	check(err)
	err = binary.Read(reader, binary.LittleEndian, out)
	check(err)
	return
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
