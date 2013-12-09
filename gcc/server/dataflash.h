// Manage the DataFlash memory chip

// low-level access
int df_init (void);
int df_isBusy (void);
int df_isEmpty (int addr, int len);
void df_readBytes (int addr, void* buf, int len);
void df_eraseEntireChip ();
void df_eraseSector (int addr);
void df_writeBytes (int addr, const void* buf, int len);

// high-level calls
unsigned df_scan (int* pPos);
int df_info (unsigned tag, int* pSize, int* pCrc);
void df_open (unsigned tag);
void df_seek (int pos);
void df_nextBytes (void* buf, int count);
const char* df_nextLine (void);
void df_create (unsigned tag);
void df_appendBytes (const void* buf, int count);
void df_appendLine (const char* buf);
void df_close (void);
