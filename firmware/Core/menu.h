#ifndef MENU_H
#define MENU_H

#include "../../common/menu_fsm.h"
#include "../../common/protocol_defs.h"

void Menu_Init(void);
void Menu_Process(InputEvent ev);

// Test control
uint8_t  Menu_IsTestRunning(void);
uint8_t  Menu_GetTestChannel(void);
uint32_t Menu_GetTestFreq(void);
uint8_t  Menu_GetTestDuty(void);
uint16_t Menu_GetTestCycles(void);
uint16_t Menu_GetTestOnSec(void);
uint16_t Menu_GetTestOffSec(void);
uint16_t Menu_GetTestRecordCount(void);
void     Menu_StartTest(void);
void     Menu_StopTest(void);
void     Menu_AddTestRecord(uint16_t cycle, uint16_t target_rpm,
                            uint16_t rpm_max, uint16_t rpm_avg,
                            uint8_t error, uint8_t startup_ok);
void     Menu_TestDone(void);

// Export
uint16_t Menu_GetExportCount(void);
uint8_t  Menu_FormatExportChunk(uint8_t *buf, uint16_t max_len);
uint8_t  Menu_ExportDone(void);

extern SystemParams g_params;
extern MenuState g_menu;
extern uint8_t test_cursor;    // TestCursorItem, for UI rendering
extern uint8_t test_running;   // for UI rendering

#endif
