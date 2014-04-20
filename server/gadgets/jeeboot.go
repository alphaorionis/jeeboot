package jeeboot

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"

	"code.google.com/p/go-uuid/uuid"
	"github.com/golang/glog"
	"github.com/jcw/flow"
)

// FIXME: bootFiles is shared between the BootData and JeeBoot gadgets
// 	apart from introducing race conditions, it also bypasses the flow approach
var bootFiles = map[string]*firmware{}

func init() {
	flow.Registry["BootData"] = func() flow.Circuitry { return &BootData{} }
	flow.Registry["JeeBoot"] = func() flow.Circuitry { return &JeeBoot{} }
}

// BootData takes the datafiles and adds them to the configuration settings.
type BootData struct {
	flow.Gadget
	In flow.Input
}

// Process data from the ReadFileText/IntelHex/BinaryFill/CrcCalc pipeline.
func (w *BootData) Run() {
	var name string
	for m := range w.In {
		switch v := m.(type) {
		case flow.Tag:
			switch v.Tag {
			case "<open>":
				name = v.Msg.(string)
				bootFiles[name] = &firmware{}
			case "<addr>":
				bootFiles[name].addr = v.Msg.(int)
			case "<crc16>":
				bootFiles[name].crc = v.Msg.(uint16)
			case "<close>":
				fw := bootFiles[name]
				glog.Infof("bootFile %s = addr %d crc 0x%04x len %d",
					name, fw.addr, fw.crc, len(fw.data))
			}
		case []byte:
			bootFiles[name].data = make([]byte, len(v))
                        copy(bootFiles[name].data, v) // FIXME: this is a work-around because
                        // all the "v" we get are the same []byte over and over just with new
                        // data inside, so all of the bootFiles[*].data end up sharing the same
                        // underlying array, which doesn't work
                        //glog.Infof("bootfile %s -> %x", name, v)
		}
	}
}

// This takes JeeBoot requests and returns the desired information as reply.
type JeeBoot struct {
	flow.Gadget
	In    flow.Input
	Cfg   flow.Input
	Out   flow.Output
	Files flow.Output

	dev string
	cfg config
}

// Start decoding JeeBoot packets.
func (w *JeeBoot) Run() {
        //glog.Infof("JeeBoot waiting for config")
	for m := range w.Cfg {
                switch m.(type) {
                case flow.Tag:
                        glog.Infof("  JeeBoot config file %s", m.(flow.Tag).Msg)
                case map[string]interface{}:
                        //glog.Infof("  JeeBoot config: %#v\n", m)
		        data, err := json.Marshal(m) // TODO: messy, encode again, then decode!
		        flow.Check(err)
		        err = json.Unmarshal(data, &w.cfg)
		        flow.Check(err)
		        glog.Infof("config: %+v", w.cfg)
		        for _, f := range w.cfg.SwIDs {
			        w.Files.Send(f)
		        }
		        w.Files.Disconnect()
                        break
                default:
                        glog.Warningf("  JeeBoot bad config: %#v", m)
                }
	}
	for m := range w.In {
		if req, ok := m.([]byte); ok {
			nodeId, reply := w.respondToRequest(req)
			if reply != nil {
				cmd := convertReplyToCmd(nodeId, reply)
				// fmt.Println("JB reply #", len(req), "->", cmd)
				w.Out.Send(cmd)
			}
		}
	}
}

func convertReplyToCmd(nodeId byte, reply interface{}) []byte {
	var buf bytes.Buffer
	err := binary.Write(&buf, binary.LittleEndian, 0xE0 | nodeId)
	flow.Check(err)
	err = binary.Write(&buf, binary.LittleEndian, reply)
	flow.Check(err)
	fmt.Printf("JB reply %x\n", buf.Bytes())
        return buf.Bytes()
	//cmd := strings.Replace(fmt.Sprintf("%v", buf.Bytes()), " ", ",", -1)
	//return cmd[1:len(cmd)-1] + ",81s" // FIXME: hard-coded 64+17!
}

type firmware struct {
	addr int
	crc  uint16
	data []byte
}

type config struct {
	SwIDs map[string]string // map SwIDs to filenames
	HwIDs map[string]struct{ Board, Group, Node, SwID float64 }
}

func (c *config) LookupHwID(hwID []byte) (board, group, node uint8) {
	key := hex.EncodeToString(hwID)
	if info, ok := c.HwIDs[key]; ok {
		board = uint8(info.Board)
		group = uint8(info.Group)
		node = uint8(info.Node)
	}
	return
}

func (c *config) LookupSwID(group, node uint8) uint16 {
	for _, h := range c.HwIDs {
		if group == uint8(h.Group) && node == uint8(h.Node) {
			return uint16(h.SwID)
		}
	}
	return 0
}

func (c *config) GetFirmware(swId uint16) *firmware {
        //for name, fw := range bootFiles {
        //        glog.V(3).Infof("bootfile %s -> %x", name, fw.data)
        //}
	filename := c.SwIDs[strconv.Itoa(int(swId))]
	glog.V(2).Infof("SwID %d -> %s 0x%x", swId, filename, bootFiles[filename].crc)
	return bootFiles[filename]
}

type pairingRequest struct {
	Variant uint8     // variant of remote node, 1..250 freely available
	Board   uint8     // type of remote node, 100..250 freely available
	Group   uint8     // current network group, 1..250 or 0 if unpaired
	NodeID  uint8     // current node ID, 1..30 or 0 if unpaired
	Check   uint16    // crc checksum over the current shared key
	HwID    [16]uint8 // unique hardware ID or 0's if not available
}

type pairingAssign struct {
	Variant uint8     // variant of remote node, 1..250 freely available
	Board   uint8     // type of remote node, 100..250 freely available
	HwID    [16]uint8 // freshly assigned hardware ID for boards which need it
}

type pairingReply struct {
	Variant uint8     // variant of remote node, 1..250 freely available
	Board   uint8     // type of remote node, 100..250 freely available
	Group   uint8     // assigned network group, 1..250
	NodeID  uint8     // assigned node ID, 1..30
	ShKey   [16]uint8 // shared key or 0's if not used
}

type upgradeRequest struct {
	Variant uint8  // variant of remote node, 1..250 freely available
	Board   uint8  // type of remote node, 100..250 freely available
	SwID    uint16 // current software ID 0 if unknown
	SwSize  uint16 // current software download size, in units of 16 bytes
	SwCheck uint16 // current crc checksum over entire download
}

type upgradeReply upgradeRequest // same layout

type downloadRequest struct {
	SwID    uint16 // current software ID
	SwIndex uint16 // current download index, as multiple of payload size
}

type downloadReply struct {
	SwIDXor uint16    // current software ID xor current download index
	Data    [64]uint8 // download payload
}

func nodeId(hdr byte) byte {
        return hdr & 0x1f
}

func (w *JeeBoot) respondToRequest(req []byte) (byte, interface{}) {
	// fmt.Printf("rtr %s %x %d\n", w.dev, req, len(req))
	switch len(req) - 1 {

        // Pairing request -- Assign HW ID if not set and network params (group/node)
	case 22:
		var preq pairingRequest
		hdr := unpackReq(req, &preq)
		// if HwID is all zeroes, we need to issue a new random value
		if preq.HwID == [16]byte{} {
			reply := pairingAssign{Board: preq.Board}
			copy(reply.HwID[:], newRandomID())
			fmt.Printf("assigning fresh hardware ID %x for board %d hdr %08b\n",
				reply.HwID, preq.Board, hdr)
			return 1, reply // using node=1 for pairing
		}
                // got a HwID, now lookup group, node, and swid
		board, group, node := w.cfg.LookupHwID(preq.HwID[:])
		glog.Infof("key %x  board %x g%di%d", preq.HwID, board, group, node)
		if board == preq.Board && group != 0 && node != 0 {
			fmt.Printf("pair %x board %d hdr %08b\n", preq.HwID, board, hdr)
			reply := pairingReply{Board: board, Group: group, NodeID: node}
			return 1, reply // using node=1 for pairing
		}
		fmt.Printf("pair %x board %d - no entry\n", preq.HwID, board)

        // Upgrade request -- Tell the node which software it's supposed to be running
	case 8:
		var ureq upgradeRequest
		hdr := unpackReq(req, &ureq)
		group, node := uint8(212), hdr&0x1F // FIXME hard-coded for now
		// upgradeRequest can be used as reply as well, it has the same fields
		reply := &ureq
		reply.SwID = w.cfg.LookupSwID(group, node)
		if fw := w.cfg.GetFirmware(reply.SwID); fw != nil {
			reply.SwSize = uint16(len(fw.data) >> 4)
			reply.SwCheck = fw.crc
			fmt.Printf("upgrade hdr=%x to SwID=%d SwCheck=%x\n",
                                hdr, reply.SwID, reply.SwCheck)
			return nodeId(hdr), reply
		}
                fmt.Printf("cannot locate firmware for g%di%d with swID=%d\n",
                        group, node, reply.SwID)

        // Download request -- Send the node a payload of software bits
	case 4:
		var dreq downloadRequest
		hdr := unpackReq(req, &dreq)
		if fw := w.cfg.GetFirmware(dreq.SwID); fw != nil {
			offset := 64 * dreq.SwIndex // FIXME hard-coded
			reply := downloadReply{SwIDXor: dreq.SwID ^ dreq.SwIndex}
			fmt.Printf("SwID=%d len%d offset=%d..%d\n", dreq.SwID, len(fw.data),
                                offset, offset+64)
                        //fmt.Printf("Firmware: %#v\n", fw)
			if int(offset+64) > len(fw.data) {
				fmt.Printf("no data at %d..%d\n", offset, offset+64)
				return nodeId(hdr), &struct{ SwIDXor uint16 }{
					SwIDXor: dreq.SwID ^ dreq.SwIndex,
				}
			}
			for i, v := range fw.data[offset : offset+64] {
				reply.Data[i] = v ^ uint8(211*i)
			}
			//fmt.Printf("download hdr=%x data=%x\n", hdr, fw.data[offset : offset+64])
			return nodeId(hdr), reply
		}

	default:
		fmt.Printf("bad req? %d b = %d\n", len(req), req)
	}

	return 0, nil
}

func unpackReq(data []byte, req interface{}) (h uint8) {
	reader := bytes.NewReader(data)
	err := binary.Read(reader, binary.LittleEndian, &h)
	flow.Check(err)
	err = binary.Read(reader, binary.LittleEndian, req)
	flow.Check(err)
	// fmt.Printf("%08b %x\n", h, req)
	return
}

func newRandomID() []byte {
	// use the uuid package (overkill?) to come up with 16 random bytes
	r, _ := hex.DecodeString(strings.Replace(uuid.New(), "-", "", -1))
	return r
}
