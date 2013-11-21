// Boot packet types, exchanged between remote nodes and the boot server.
// -jcw, 2013-11-17

struct AnnounceRequest {
  uint16_t type;
  uint8_t group;
  uint8_t nodeId;
  uint8_t hwId [16];
  uint16_t check;
};

struct AnnounceReply {
  uint16_t type;
  uint8_t group;
  uint8_t nodeId;
  uint8_t shKey [16];
};

struct BootRequest {
  uint16_t type;
  uint16_t swId;
  uint16_t swSize;
  uint16_t swCheck;
};

struct BootReply {
  uint16_t type;
  uint16_t swId;
  uint16_t swSize;
  uint16_t swCheck;
};

struct DataRequest {
  uint16_t swId;
  uint16_t swIndex;
};

struct DataReply {
  uint16_t swIdXor;
  uint8_t data [BOOT_DATA_MAX];
};
