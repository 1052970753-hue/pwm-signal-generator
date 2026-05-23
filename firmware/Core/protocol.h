#ifndef FW_PROTOCOL_H
#define FW_PROTOCOL_H

#include "../../common/protocol_defs.h"
#include "system_config.h"

void Protocol_Init(void);
void Protocol_SendStatus(StatusData *status);
void Protocol_Process(void);  // Call in main loop to handle incoming frames

#endif
