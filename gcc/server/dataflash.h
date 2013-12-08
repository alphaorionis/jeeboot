// Manage the DataFlash memory chip

// low-level access
int df_init ();
int df_isBusy ();
void df_eraseEntireChip ();
void df_eraseSector (int addr);
void df_writeBytes (int addr, const void* buf, int len);
void df_readBytes (int addr, void* buf, int len);
