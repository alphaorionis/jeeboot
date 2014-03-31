package jeeboot

import (
	"encoding/json"

	"github.com/jcw/flow"
)

var configDemo = `{
	"swids": {
        "1001": "../firmware/blinkAvr1.hex"
	},
	"hwids": {
		"06300301c48461aeedb09351061900f5": {
	        "board": 2, "group": 212, "node": 17, "swid": 1001
	  }
	}
}`

func ExampleJeeBoot() {
	var any interface{}
	err := json.Unmarshal([]byte(configDemo), &any)
	flow.Check(err)

	g := flow.NewCircuit()
	g.Add("jb", "JeeBoot")
	g.Feed("jb.Cfg", any)
	g.Feed("jb.In", []byte{
		224, 0, 2, 212, 17, 190, 240, 6, 48, 3,
		1, 196, 132, 97, 174, 237, 176, 147, 81, 6,
		25, 0, 245,
	})
	g.Feed("jb.In", []byte{
		177, 0, 2, 1, 0, 17, 0, 99, 36,
	})
	g.Feed("jb.In", []byte{
		177, 1, 0, 0, 0,
	})
	g.Run()
	// Output:
	// Lost string: ../firmware/blinkAvr1.hex
	// pair 06300301c48461aeedb09351061900f5 board 2 hdr 11100000
	// JB reply 0002d41100000000000000000000000000000000
	// Lost string: 0,2,212,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,81s
}
