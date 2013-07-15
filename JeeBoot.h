// JeeBoot common definitions

typedef struct {
  word remoteID;
  word sketchBlocks;
  word sketchCRC;
} BootReply;

typedef struct {
  word info;
  byte data[64];
} DataReply;

typedef struct {
  byte version;
  byte group;
  byte nodeId;
  word type;
} PairRequest;

typedef struct {
  byte version;
  byte group;
  byte nodeId;
  word type;
  byte sharedKey [16];
} PairReply;
