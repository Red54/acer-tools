typedef unsigned short crc16ccit_ctx;

/*
 * Creates new CRC16CCIT context
 */
void crc16ccit_new(crc16ccit_ctx* ctx);

/*
 * Recalculate value
 */
void crc16ccit_update(crc16ccit_ctx* ctx, unsigned char value);

/*
 * Return CRC16CCIT digest
 */
unsigned short crc16ccit_digest(crc16ccit_ctx *ctx);

