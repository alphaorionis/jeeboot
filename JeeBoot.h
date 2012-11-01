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
