package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"

	"code.google.com/p/go-uuid/uuid"
	"github.com/jcw/jeebus"
)

const FIRMWARE_PREFIX = "./files/firmware/" // location of hex files to serve

var client *jeebus.Client

func main() {
	log.Printf("%x", newRandomId())
	if len(os.Args) <= 1 {
		log.Fatalf("usage: jeeboot serialport")
	}

	boots(os.Args[1]) // expect "usb-..." as arg
}

func boots(dev string) {
	client = jeebus.NewClient(nil)

	config := loadConfig()
	fw := loadAllFirmware(config)

	client.Register("rd/RF12demo/"+dev, &JeeBootService{dev, config, fw})

	msg := map[string]interface{}{"text": "8b 212g 31i 1c"}
	client.Publish("if/RF12demo/"+dev, msg)
	<-client.Done
}

type HwIdStruct struct{ Board, Group, Node, SwId float64 }
type SwIdStruct struct{ File string }

type Config struct {
	// map 16-byte hardware ID to the assigned pairing info
	HwId map[string]HwIdStruct
	// map each swId to a filename
	Firmware map[string]SwIdStruct
}

func (c *Config) LookupHwId(hwId []byte) (board, group, node uint8) {
	key := hex.EncodeToString(hwId)
	if info, ok := c.HwId[key]; ok {
		board = uint8(info.Board)
		group = uint8(info.Group)
		node = uint8(info.Node)
	}
	return
}

func (c *Config) LookupSwId(group, node uint8) uint16 {
	for _, h := range c.HwId {
		if group == uint8(h.Group) && node == uint8(h.Node) {
			return uint16(h.SwId)
		}
	}
	return 0
}

func loadConfig() (config Config) {
	// TODO this sort of dynamic decoding is still very tedious

	hkeys, err := client.Call("db-keys", "/jeeboot/hwid/")
	check(err)
	config.HwId = make(map[string]HwIdStruct)
	for _, k := range hkeys.([]interface{}) {
		v, err := client.Call("db-get", "/jeeboot/hwid/" + k.(string))
		check(err)
		log.Println(k, "=", v)
		var hs HwIdStruct
		err = json.Unmarshal([]byte(v.(string)), &hs)
		check(err)
		config.HwId[k.(string)] = hs
	}

	fkeys, err := client.Call("db-keys", "/jeeboot/firmware/")
	check(err)
	config.Firmware = make(map[string]SwIdStruct)
	for _, k := range fkeys.([]interface{}) {
		v, err := client.Call("db-get", "/jeeboot/firmware/" + k.(string))
		check(err)
		log.Println(k, "=", v)
		var ss SwIdStruct
		err = json.Unmarshal([]byte(v.(string)), &ss)
		check(err)
		config.Firmware[k.(string)] = ss
	}

	log.Printf("CONFIG %d hw %d fw", len(config.HwId), len(config.Firmware))
	return
}

type Firmware struct {
	name string
	crc  uint16
	data []byte
}

func loadAllFirmware(config Config) map[uint16]Firmware {
	fw := make(map[uint16]Firmware)
	for key, name := range config.Firmware {
		swId, err := strconv.Atoi(key)
		check(err)
		fw[uint16(swId)] = readFirmware(name.File)
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
	fw     map[uint16]Firmware
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

func (s *JeeBootService) Send(reply interface{}) {
	var buf bytes.Buffer
	err := binary.Write(&buf, binary.LittleEndian, reply)
	check(err)
	cmd := strings.Replace(fmt.Sprintf("%v", buf.Bytes()), " ", ",", -1)
	// log.Printf("reply %s ,0s", cmd)
	msg := map[string]string{"text": cmd[1:len(cmd)-1] + ",0s"}
	client.Publish("if/RF12demo/"+s.dev, msg)
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
		hdr := s.unpackReq(req, &preq)
		board, group, node := s.config.LookupHwId(preq.HwId[:])
		log.Printf("pairing %X board %d hdr %08b", preq.HwId, board, hdr)
		reply := PairingReply{Board: board, Group: group, NodeId: node}
		s.Send(reply)

	case 8:
		var ureq UpgradeRequest
		hdr := s.unpackReq(req, &ureq)
		group, node := uint8(212), hdr&0x1F // FIXME hard-coded for now
		// UpgradeRequest can be used as reply as well, it has the same fields
		reply := &ureq
		reply.SwId = s.config.LookupSwId(group, node)
		fw := s.fw[reply.SwId]
		reply.SwSize = uint16(len(fw.data) >> 4)
		reply.SwCheck = fw.crc
		log.Printf("upgrade %v hdr %08b", reply, hdr)
		s.Send(reply)

	case 4:
		var dreq DownloadRequest
		hdr := s.unpackReq(req, &dreq)
		fw := s.fw[dreq.SwId]
		offset := 64 * dreq.SwIndex // FIXME hard-coded
		reply := DownloadReply{SwIdXor: dreq.SwId ^ dreq.SwIndex}
		log.Println("len", len(fw.data), "offset", offset, offset+64)
		for i, v := range fw.data[offset : offset+64] {
			reply.Data[i] = v ^ uint8(211*i)
		}
		log.Printf("download hdr %08b", hdr)
		s.Send(reply)

	default:
		log.Printf("bad req? %d b = %d", len(req), req)
	}
}

func (s *JeeBootService) unpackReq(data []byte, req interface{}) (h uint8) {
	reader := bytes.NewReader(data)
	err := binary.Read(reader, binary.LittleEndian, &h)
	check(err)
	err = binary.Read(reader, binary.LittleEndian, req)
	check(err)
	log.Printf("%08b %X\n", h, req)
	return
}

func newRandomId() []byte {
	// use the uuid package (overkill?) to come up with 16 random bytes
	r, _ := hex.DecodeString(strings.Replace(uuid.New(), "-", "", -1))
	return r
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
