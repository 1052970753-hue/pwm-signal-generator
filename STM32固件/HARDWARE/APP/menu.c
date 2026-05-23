/*
 * menu.c — 5 模式菜单状态机
 * ══════════════════════════════════════════════════════════════
 *
 * 【5 种模式 — 双击循环切换】
 *   MODE_PWM_FG (0): PWM 双通道 + 频率计 (5 个参数项)
 *   MODE_FG     (1): 纯频率计模式 (1 个参数: 分频系数)
 *   MODE_CH1    (2): CH1 单通道 PWM (3 个参数: 频率/占空比/使能)
 *   MODE_CH2    (3): CH2 单通道 PWM (同 CH1)
 *   MODE_TEST   (4): 自动测试模式 (7 个参数项 + 运行状态)
 *
 * 【两种操作模式】
 *   调节模式 (selected=0): 旋转编码器调节当前光标项的值
 *   选择模式 (selected=1): 旋转编码器移动光标到不同参数项, 短按退出
 *   长按在调节模式下进入选择模式, 短按在选择模式下退出
 *
 * 【输入事件映射】
 *   EVENT_CW          → 旋转编码器顺时针 (值 +1 或光标 +1)
 *   EVENT_CCW         → 旋转编码器逆时针 (值 -1 或光标 -1)
 *   EVENT_CLICK       → 短按 (切换使能 / 退出选择模式 / 启动测试)
 *   EVENT_DOUBLE_CLICK → 双击 (切换到下一个模式)
 *   EVENT_LONG_PRESS  → 长按 (进入选择模式)
 *
 * 【测试记录环形缓冲区】
 *   最多存储 200 条 TestRecord, 每条包含: 循环编号/目标RPM/最大RPM/平均RPM/异常标志/启动标志
 *   测试完成后可通过 CSV 导出 (分块发送)
 *
 * 【CSV 导出格式】
 *   表头: "cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n"
 *   数据: 每行一条记录, 逐块发送 (每块最多 64 字节)
 *   export_index=0 发送表头, 1~count 发送数据行, >count 返回 0 (结束)
 */
#include "menu.h"
#include "system_config.h"
#include <string.h>

/* ════════════════════════════════════════════════════════════
 *  全局状态变量
 * ════════════════════════════════════════════════════════════ */
SystemParams g_params;      // 系统参数 (CH1/CH2 频率/占空比/使能 + FG 分频)
MenuState g_menu;           // 菜单状态 (光标位置/当前模式/选择模式标志/脏标志)

/* ════════════════════════════════════════════════════════════
 *  测试配置参数 — 非静态, 供 protocol.c 通过 getter/setter 访问
 * ════════════════════════════════════════════════════════════
 *  这些参数由用户在 TEST 模式下通过旋钮设置, 或由 PC 端通过 CMD_SET_TEST 写入
 */
u8  test_channel = 1;       // 测试通道 (1=CH1, 2=CH2)
u32 test_freq = 1000;       // 测试频率 (Hz), 范围 1~100000
u8  test_duty = 50;         // 测试占空比 (%), 范围 0~100
u16 test_cycles = 10;       // 循环次数, 范围 1~999
u16 test_on_sec = 5;        // ON 阶段持续时间 (秒), 范围 1~60
u16 test_off_sec = 3;       // OFF 阶段持续时间 (秒), 范围 1~60
u8  test_running = 0;       // 测试运行中标志 (0=停止, 1=运行中)

/* ════════════════════════════════════════════════════════════
 *  测试记录环形缓冲区
 * ════════════════════════════════════════════════════════════
 *  每完成一个测试循环, 由 main.c 的测试状态机调用 Menu_AddTestRecord() 添加记录
 *  最多存储 TEST_BUF_SIZE (200) 条记录
 *  每条 TestRecord 包含:
 *    cycle:       循环编号 (从 1 开始)
 *    target_rpm:  目标 RPM (由测试频率计算)
 *    rpm_max:     ON 阶段最大 RPM
 *    rpm_avg:     ON 阶段平均 RPM
 *    error:       异常标志 (1=RPM=0 或偏差>20%)
 *    startup_ok:  启动成功标志 (1=RPM>0)
 */
typedef struct {
    u16 cycle;          // 循环编号
    u16 target_rpm;     // 目标 RPM (根据频率计算)
    u16 rpm_max;        // ON 阶段最大 RPM
    u16 rpm_avg;        // ON 阶段平均 RPM
    u8  error;          // 1=异常 (RPM=0 或偏差>20%)
    u8  startup_ok;     // 1=成功启动 (RPM>0)
} TestRecord;

static TestRecord test_records[TEST_BUF_SIZE];  // 记录缓冲区 (最多 200 条)
static u16 test_record_count = 0;               // 当前记录数

/* ── CSV 导出状态 ──
 * export_index: 当前导出位置
 *   0 = 尚未导出, 下次发送表头
 *   1 = 已发完表头, 下次发送第 1 条记录 (index=0)
 *   N = 已发完第 N-1 条记录, 下次发送第 N 条 (index=N-1)
 *   > test_record_count = 导出完毕
 */
static u16 export_index = 0;

/* ════════════════════════════════════════════════════════════
 *  菜单初始化
 * ════════════════════════════════════════════════════════════
 *  设置默认模式为 PWM-FG, 光标默认在 CH1 占空比位置
 *  初始化默认系统参数 (CH1/CH2 均为 1kHz/50%/关闭, FG 分频=2)
 */
void Menu_Init(void) {
    g_menu.cursor = ITEM_CH1_DUTY;  // 默认光标在 CH1 占空比
    g_menu.selected = 0;            // 调节模式 (非选择模式)
    g_menu.dirty = 1;               // 首次标记为脏, 触发 PWM 初始化
    g_menu.mode = MODE_PWM_FG;      // 默认进入 PWM-FG 模式
    g_menu.blink = 0;               // 光标闪烁状态

    // 默认系统参数
    g_params.ch1_freq_hz = 1000;    // CH1 默认 1kHz
    g_params.ch1_duty_pct = 50;     // CH1 默认 50%
    g_params.ch1_enabled = 0;       // CH1 默认关闭
    g_params.ch2_freq_hz = 1000;    // CH2 默认 1kHz
    g_params.ch2_duty_pct = 50;     // CH2 默认 50%
    g_params.ch2_enabled = 0;       // CH2 默认关闭
    g_params.fg_div = 2;            // FG 默认 2 分频
    g_params.fg_pulses_per_rev = 2; // 每转脉冲数 (备用)
    g_params.vsp_voltage_x100 = 0;  // VSP 默认 0V
    g_params.vsp_enabled = 0;       // VSP 默认关闭
    g_params.test_on_method = 0;    // 测试默认 PWM 开关
}

/* ════════════════════════════════════════════════════════════
 *  PWM-FG 模式: 光标值读写 (5 个参数项)
 * ════════════════════════════════════════════════════════════
 *  光标项枚举 (由 menu.h 定义):
 *    ITEM_CH1_FREQ (0): CH1 频率   范围 1~100000 Hz
 *    ITEM_CH1_DUTY (1): CH1 占空比 范围 0~100 %
 *    ITEM_CH2_FREQ (2): CH2 频率   范围 1~100000 Hz
 *    ITEM_CH2_DUTY (3): CH2 占空比 范围 0~100 %
 *    ITEM_FG_DIV   (4): FG 分频    范围 1~99
 */

/* ── 读取当前光标项的值 ──
 * 根据 g_menu.cursor 的值, 返回对应参数的当前值
 * 返回类型为 u32, 统一接口
 */
static u32 get_cursor_value(void) {
    switch (g_menu.cursor) {
        case ITEM_CH1_FREQ: return g_params.ch1_freq_hz;   // CH1 频率
        case ITEM_CH1_DUTY: return g_params.ch1_duty_pct;  // CH1 占空比
        case ITEM_CH2_FREQ: return g_params.ch2_freq_hz;   // CH2 频率
        case ITEM_CH2_DUTY: return g_params.ch2_duty_pct;  // CH2 占空比
        case ITEM_FG_DIV:   return g_params.fg_div;        // FG 分频系数
        default: return 0;
    }
}

/* ── 设置当前光标项的值 (带自动限幅) ──
 * 根据 g_menu.cursor 的值, 写入对应参数
 * 自动将值限制在合法范围内:
 *   频率: 1~100000 Hz
 *   占空比: 0~100 %
 *   分频: 1~99
 */
static void set_cursor_value(u32 val) {
    switch (g_menu.cursor) {
        case ITEM_CH1_FREQ:
            if (val < 1) val = 1;                       // 下限保护
            if (val > 100000) val = 100000;              // 上限保护
            g_params.ch1_freq_hz = val;
            break;
        case ITEM_CH1_DUTY:
            if (val > 100) val = 100;                    // 上限保护 (下限为 0, u32 自然满足)
            g_params.ch1_duty_pct = val;
            break;
        case ITEM_CH2_FREQ:
            if (val < 1) val = 1;
            if (val > 100000) val = 100000;
            g_params.ch2_freq_hz = val;
            break;
        case ITEM_CH2_DUTY:
            if (val > 100) val = 100;
            g_params.ch2_duty_pct = val;
            break;
        case ITEM_FG_DIV:
            if (val < 1) val = 1;
            if (val > 99) val = 99;
            g_params.fg_div = val;
            break;
        default: break;
    }
}

/* ════════════════════════════════════════════════════════════
 *  CH1/CH2 单通道模式: 光标值读写 (3 个参数项)
 * ════════════════════════════════════════════════════════════
 *  光标项:
 *    cursor=0: 频率   范围 1~100000 Hz
 *    cursor=1: 占空比 范围 0~100 %
 *    cursor=2: 使能   (由短按切换, 旋钮无操作)
 *  channel 参数区分 CH1 和 CH2
 */

/* ── 读取单通道模式下当前光标项的值 ──
 * cursor=0 返回频率, cursor=1 返回占空比, cursor=2 返回 0 (使能由短按控制)
 */
static u32 get_ch_cursor_value(u8 channel) {
    u8 c = g_menu.cursor;
    if (channel == 1) {
        if (c == 0) return g_params.ch1_freq_hz;         // CH1 频率
        if (c == 1) return g_params.ch1_duty_pct;        // CH1 占空比
    } else {
        if (c == 0) return g_params.ch2_freq_hz;         // CH2 频率
        if (c == 1) return g_params.ch2_duty_pct;        // CH2 占空比
    }
    return 0;  // cursor=2 (使能) 无旋钮值
}

/* ── 设置单通道模式下当前光标项的值 (带限幅) ── */
static void set_ch_cursor_value(u8 channel, u32 val) {
    u8 c = g_menu.cursor;
    if (channel == 1) {
        if (c == 0) { if (val<1) val=1; if (val>100000) val=100000; g_params.ch1_freq_hz=val; }   // CH1 频率限幅
        else if (c == 1) { if (val>100) val=100; g_params.ch1_duty_pct=val; }                       // CH1 占空比限幅
    } else {
        if (c == 0) { if (val<1) val=1; if (val>100000) val=100000; g_params.ch2_freq_hz=val; }   // CH2 频率限幅
        else if (c == 1) { if (val>100) val=100; g_params.ch2_duty_pct=val; }                       // CH2 占空比限幅
    }
}

/* ════════════════════════════════════════════════════════════
 *  测试模式: 光标值设置 (7 个参数项)
 * ════════════════════════════════════════════════════════════
 *  光标项 (由 menu.h 中的 TEST_ITEM_* 定义):
 *    TEST_ITEM_CHANNEL (0): 测试通道 (1/2 切换)
 *    TEST_ITEM_FREQ    (1): 测试频率 1~100000 Hz
 *    TEST_ITEM_DUTY    (2): 测试占空比 0~100 %
 *    TEST_ITEM_CYCLES  (3): 循环次数 1~999
 *    TEST_ITEM_ON_TIME (4): ON 时间 1~60 秒
 *    TEST_ITEM_OFF_TIME(5): OFF 时间 1~60 秒
 *    TEST_ITEM_START   (6): 启动按钮 (无旋钮操作)
 */

/* ── 设置测试模式下当前光标项的值 ──
 * delta > 0: 增加 (顺时针旋转)
 * delta < 0: 减少 (逆时针旋转)
 *
 * 【有符号/无符号算术处理】
 *   参数类型不同 (u8/u16/u32), 统一用 s32 delta 传入
 *   增加时: 直接加 delta (转换为对应类型), 检查上限
 *   减少时: 将 -delta 转为正数 d, 比较当前值与 d:
 *     - 如果 val > d: val - d (正常减法)
 *     - 如果 val <= d: 取最小值 (1 或 0, 取决于参数)
 *   这种方式避免了 unsigned 下溢回绕到极大值的问题
 */
static void set_test_cursor_value(s32 delta) {
    switch (g_menu.cursor) {
        case TEST_ITEM_CHANNEL:
            // 通道切换: 1→2 或 2→1 (直接翻转, delta 仅区分方向)
            test_channel = (test_channel == 1) ? 2 : 1;
            break;
        case TEST_ITEM_FREQ:
            if (delta > 0) {
                test_freq += (u32)delta;                            // 顺时针: 增加频率
                if (test_freq > 100000) test_freq = 100000;         // 上限保护
            } else {
                u32 d = (u32)(-delta);                              // 将负增量转为正数
                test_freq = (test_freq > d) ? test_freq - d : 1;    // 防止下溢, 最小为 1
            }
            break;
        case TEST_ITEM_DUTY:
            if (delta > 0) {
                test_duty += (u8)delta;                             // 增加占空比
                if (test_duty > 100) test_duty = 100;               // 上限 100%
            } else {
                u8 d = (u8)(-delta);
                test_duty = (test_duty > d) ? test_duty - d : 0;    // 最小为 0%
            }
            break;
        case TEST_ITEM_CYCLES:
            if (delta > 0) {
                test_cycles += (u16)delta;                          // 增加循环次数
                if (test_cycles > 999) test_cycles = 999;           // 上限 999
            } else {
                u16 d = (u16)(-delta);
                test_cycles = (test_cycles > d) ? test_cycles - d : 1;  // 最小为 1
            }
            break;
        case TEST_ITEM_ON_TIME:
            if (delta > 0) {
                test_on_sec += (u16)delta;                          // 增加 ON 时间
                if (test_on_sec > 60) test_on_sec = 60;             // 上限 60 秒
            } else {
                u16 d = (u16)(-delta);
                test_on_sec = (test_on_sec > d) ? test_on_sec - d : 1;  // 最小为 1 秒
            }
            break;
        case TEST_ITEM_OFF_TIME:
            if (delta > 0) {
                test_off_sec += (u16)delta;                         // 增加 OFF 时间
                if (test_off_sec > 60) test_off_sec = 60;           // 上限 60 秒
            } else {
                u16 d = (u16)(-delta);
                test_off_sec = (test_off_sec > d) ? test_off_sec - d : 1;  // 最小为 1 秒
            }
            break;
        case TEST_ITEM_ON_METHOD:
            // ON 方式切换: 0=PWM → 1=继电器 → 2=两者 → 0=PWM
            if (delta > 0) {
                g_params.test_on_method++;
                if (g_params.test_on_method > 2) g_params.test_on_method = 0;
            } else {
                if (g_params.test_on_method == 0)
                    g_params.test_on_method = 2;
                else
                    g_params.test_on_method--;
            }
            break;
        default: break;  // TEST_ITEM_START 无旋钮操作
    }
}

/* ════════════════════════════════════════════════════════════
 *  公开接口: 测试参数 getter/setter
 * ════════════════════════════════════════════════════════════
 *  供 protocol.c 和 main.c 使用, 避免直接访问静态变量
 */

// 查询测试是否正在运行
u8  Menu_IsTestRunning(void) { return test_running; }
// 获取测试通道 (1 或 2)
u8  Menu_GetTestChannel(void) { return test_channel; }
// 获取测试频率 (Hz)
u32 Menu_GetTestFreq(void) { return test_freq; }
// 获取测试占空比 (%)
u8  Menu_GetTestDuty(void) { return test_duty; }
// 获取测试循环次数
u16 Menu_GetTestCycles(void) { return test_cycles; }
// 获取 ON 持续时间 (秒)
u16 Menu_GetTestOnSec(void) { return test_on_sec; }
// 获取 OFF 持续时间 (秒)
u16 Menu_GetTestOffSec(void) { return test_off_sec; }
// 获取已记录的测试数据条数
u16 Menu_GetTestRecordCount(void) { return test_record_count; }
// 获取测试 ON 方式 (0=PWM, 1=继电器, 2=两者)
u8  Menu_GetTestOnMethod(void) { return g_params.test_on_method; }

/* ── PC 端设置测试参数 (含边界检查) ──
 * 由 protocol.c 的 CMD_SET_TEST 调用
 * 每个参数独立校验, 非法则取默认值
 */
void Menu_SetTestConfig(TestConfig *cfg) {
    test_channel = (cfg->channel >= 1 && cfg->channel <= 2) ? cfg->channel : 1;           // 通道必须为 1 或 2
    test_freq = (cfg->freq_hz >= 1 && cfg->freq_hz <= 100000) ? cfg->freq_hz : 1000;     // 频率 1~100000, 默认 1000
    test_duty = (cfg->duty_pct <= 100) ? cfg->duty_pct : 50;                              // 占空比 0~100, 默认 50
    test_cycles = (cfg->cycles >= 1) ? cfg->cycles : 1;                                    // 循环至少 1 次
    test_on_sec = (cfg->on_time_sec >= 1) ? cfg->on_time_sec : 1;                         // ON 至少 1 秒
    test_off_sec = (cfg->off_time_sec >= 1) ? cfg->off_time_sec : 1;                      // OFF 至少 1 秒
    g_params.test_on_method = (cfg->on_method <= 2) ? cfg->on_method : 0;                 // ON 方式 0~2
}

/* ── 启动测试 ──
 * 设置 test_running=1, 清空记录缓冲区和导出索引
 * 实际测试逻辑由 main.c 的测试状态机执行
 */
void Menu_StartTest(void) {
    test_running = 1;           // 置运行标志
    test_record_count = 0;      // 清空历史记录
    export_index = 0;           // 重置导出位置
}

/* ── 停止测试 ──
 * 由 protocol.c 的 CMD_STOP_TEST 或 UI 短按调用
 */
void Menu_StopTest(void) {
    test_running = 0;           // 清除运行标志
}

/* ── 添加一条测试记录到环形缓冲区 ──
 * 由 main.c 的测试状态机在每个循环结束时调用
 * 如果缓冲区已满 (>=TEST_BUF_SIZE), 新记录将被丢弃
 */
void Menu_AddTestRecord(u16 cycle, u16 target_rpm, u16 rpm_max, u16 rpm_avg,
                        u8 error, u8 startup_ok) {
    if (test_record_count < TEST_BUF_SIZE) {               // 缓冲区未满才添加
        TestRecord *r = &test_records[test_record_count++];
        r->cycle = cycle;                                   // 循环编号
        r->target_rpm = target_rpm;                         // 目标 RPM
        r->rpm_max = rpm_max;                               // 最大 RPM
        r->rpm_avg = rpm_avg;                               // 平均 RPM
        r->error = error;                                   // 异常标志
        r->startup_ok = startup_ok;                         // 启动成功标志
    }
}

/* ── 测试完成回调 ──
 * 由 main.c 测试状态机在所有循环完成后调用
 */
void Menu_TestDone(void) {
    test_running = 0;
}

/* ════════════════════════════════════════════════════════════
 *  CSV 导出 — 逐块发送机制
 * ════════════════════════════════════════════════════════════
 *  每次调用 Menu_FormatExportChunk() 返回一块 CSV 数据
 *  数据流:
 *    export_index=0 → 表头 "cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n"
 *    export_index=1 → 第 1 条记录 (test_records[0])
 *    export_index=N → 第 N 条记录 (test_records[N-1])
 *    export_index>count → 返回 0 (导出完毕)
 *  每块数据不超过 max_len (通常 64 字节), 可安全放入一帧传输
 */

// 获取总记录数 (供 protocol.c 判断导出量)
u16 Menu_GetExportCount(void) { return test_record_count; }

/* ── 格式化一块 CSV 数据 ──
 * buf: 输出缓冲区
 * max_len: 缓冲区最大长度 (通常 64)
 * 返回: 实际写入字节数, 0 表示导出完毕
 *
 * CSV 格式 (每条记录一行):
 *   cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n
 * 例: "3,3000,2980,2950,0,1\r\n"
 */
u8 Menu_FormatExportChunk(u8 *buf, u16 max_len) {
    // 第一块: 返回 CSV 表头
    if (export_index == 0) {
        const char *header = "cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n";
        u16 len = 0;
        while (header[len] && len < max_len - 1) { buf[len] = header[len]; len++; }
        export_index++;                                     // 索引递增, 下次返回数据行
        return len;
    }
    // 所有记录已导出完毕
    if (export_index - 1 >= test_record_count) return 0;

    // 获取当前要导出的记录
    TestRecord *r = &test_records[export_index - 1];
    u16 len = 0;

    /* 【内联宏 — 用于在缓冲区中输出字符和数字】
     * PUT_CHAR(c): 输出一个字符到 buf, 仅在 len < max_len 时写入 (防溢出)
     * PUT_NUM(n):  将 u32 整数转为十进制字符串输出 (无前导零)
     *   使用临时数组 tmp[] 反向存储数字, 再正序写入 buf
     */
    #define PUT_CHAR(c) if (len < max_len) buf[len++] = (c)
    #define PUT_NUM(n) do { \
        char tmp[12]; u8 t=0; u32 v=(n); \
        if(v==0) tmp[t++]='0'; \
        while(v>0){tmp[t++]='0'+v%10;v/=10;} \
        while(t>0) PUT_CHAR(tmp[--t]); \
    } while(0)

    // 逐字段输出: cycle,rpm_max,rpm_avg,error,startup_ok
    PUT_NUM(r->cycle); PUT_CHAR(',');                       // 循环编号,
    PUT_NUM(r->target_rpm); PUT_CHAR(',');                  // 目标RPM,
    PUT_NUM(r->rpm_max); PUT_CHAR(',');                     // 最大RPM,
    PUT_NUM(r->rpm_avg); PUT_CHAR(',');                     // 平均RPM,
    PUT_NUM(r->error); PUT_CHAR(',');                       // 异常标志,
    PUT_NUM(r->startup_ok);                                 // 启动标志
    PUT_CHAR('\r'); PUT_CHAR('\n');                          // 换行符 \r\n

    #undef PUT_NUM
    #undef PUT_CHAR

    export_index++;                                         // 索引递增
    return len;                                             // 返回实际写入字节数
}

/* ── 检查导出是否完毕 ──
 * 返回 1 表示所有记录已导出, 0 表示还有数据
 */
u8 Menu_ExportDone(void) {
    return (export_index - 1 >= test_record_count);
}

/* ════════════════════════════════════════════════════════════
 *  主事件处理函数 — 核心状态机
 * ════════════════════════════════════════════════════════════
 *  由 main.c 的 Encoder_Poll() 检测到输入事件后调用
 *  处理流程:
 *    1. EVENT_NONE → 直接返回
 *    2. EVENT_DOUBLE_CLICK → 切换模式
 *    3. 测试运行中 → 仅短按可停止
 *    4. 选择模式 → 旋钮移动光标, 短按退出
 *    5. 调节模式 → 根据当前模式分别处理
 */
void Menu_Process(InputEvent ev) {
    if (ev == EVENT_NONE) return;                          // 无事件, 不处理
    g_menu.dirty = 1;                                      // 标记参数已修改, main.c 将更新 PWM

    /* ── 双击: 循环切换模式 ──
     * 模式编号 0~NUM_MODES-1, 取模循环
     * 切换后光标归零, 回到调节模式
     */
    if (ev == EVENT_DOUBLE_CLICK) {
        g_menu.mode = (g_menu.mode + 1) % NUM_MODES;      // 切换到下一个模式
        g_menu.cursor = 0;                                 // 光标归零
        g_menu.selected = 0;                               // 回到调节模式
        return;
    }

    /* ── 测试运行中: 仅短按可停止测试 ──
     * 运行中屏蔽所有旋钮和其他按键操作
     * 防止用户误操作修改测试参数
     */
    if (g_menu.mode == MODE_TEST && test_running) {
        if (ev == EVENT_CLICK) {
            test_running = 0;                              // 短按停止测试
        }
        return;                                            // 其他事件全部忽略
    }

    /* ── 选择模式: 旋钮移动光标, 短按退出 ──
     * selected=1 表示在选择模式
     * 旋钮控制光标在参数项之间移动 (循环)
     * 短按退出选择模式, 回到调节模式
     * 光标上限取决于当前模式:
     *   MODE_TEST:    NUM_TEST_ITEMS (7 项)
     *   MODE_PWM_FG:  NUM_ITEMS (5 项)
     *   MODE_CH1/CH2: 3 项 (频率/占空比/使能)
     */
    if (g_menu.selected) {
        u8 max_items;
        if (g_menu.mode == MODE_TEST)
            max_items = NUM_TEST_ITEMS;                    // 测试模式: 8 个参数项
        else if (g_menu.mode == MODE_PWM_FG)
            max_items = NUM_ITEMS;                         // PWM-FG 模式: 5 个参数项
        else if (g_menu.mode == MODE_VSP)
            max_items = NUM_VSP_ITEMS;                     // VSP 模式: 2 个参数项
        else
            max_items = 3;                                 // CH1/CH2 模式: 3 个参数项

        switch (ev) {
            case EVENT_CW:
                g_menu.cursor = (g_menu.cursor + 1) % max_items;              // 光标前进 (循环)
                break;
            case EVENT_CCW:
                g_menu.cursor = (g_menu.cursor + max_items - 1) % max_items;  // 光标后退 (循环)
                break;
            case EVENT_CLICK:
                g_menu.selected = 0;                       // 短按退出选择模式
                break;
            default: break;
        }
        return;
    }

    /* ════════════════════════════════════════════
     *  调节模式: 根据当前模式分派事件处理
     * ════════════════════════════════════════════ */
    switch (g_menu.mode) {

        /* ── MODE_PWM_FG: 双通道 + 频率计 ──
         * 5 个参数项: CH1频率/CH1占空比/CH2频率/CH2占空比/FG分频
         * 旋钮: 调节当前光标项的值 (+1/-1)
         * 短按: 根据光标位置切换 CH1 或 CH2 的使能
         * 长按: 进入选择模式
         */
        case MODE_PWM_FG:
            switch (ev) {
                case EVENT_CW:
                    set_cursor_value(get_cursor_value() + 1);     // 顺时针: 值 +1
                    break;
                case EVENT_CCW: {
                    u32 v = get_cursor_value();
                    if (v > 0) set_cursor_value(v - 1);          // 逆时针: 值 -1 (防下溢)
                    break;
                }
                case EVENT_CLICK:
                    // 光标在 CH1 区域 (0~1) → 切换 CH1 使能
                    // 光标在 CH2 区域 (2~3) → 切换 CH2 使能
                    if (g_menu.cursor <= ITEM_CH1_DUTY)
                        g_params.ch1_enabled = !g_params.ch1_enabled;
                    else if (g_menu.cursor <= ITEM_CH2_DUTY)
                        g_params.ch2_enabled = !g_params.ch2_enabled;
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;                         // 长按进入选择模式
                    break;
                default: break;
            }
            break;

        /* ── MODE_FG: 纯频率计模式 ──
         * 仅 1 个可调参数: FG 分频系数 (1~99)
         * 旋钮: 分频系数 +1/-1
         * 长按: 进入选择模式 (但只有 1 项, 实际无意义)
         */
        case MODE_FG:
            switch (ev) {
                case EVENT_CW:
                    g_params.fg_div++;                           // 分频系数 +1
                    if (g_params.fg_div > 99) g_params.fg_div = 99;  // 上限保护
                    break;
                case EVENT_CCW:
                    if (g_params.fg_div > 1) g_params.fg_div--;  // 分频系数 -1 (最小为 1)
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        /* ── MODE_CH1: CH1 单通道模式 ──
         * 3 个参数项: 频率/占空比/使能
         * 旋钮: 调节频率或占空比 (使能由短按切换)
         * 短按: 切换 CH1 使能
         * 长按: 进入选择模式
         */
        case MODE_CH1:
            switch (ev) {
                case EVENT_CW:
                    set_ch_cursor_value(1, get_ch_cursor_value(1) + 1);    // CH1 参数 +1
                    break;
                case EVENT_CCW: {
                    u32 v = get_ch_cursor_value(1);
                    if (v > 0) set_ch_cursor_value(1, v - 1);              // CH1 参数 -1
                    break;
                }
                case EVENT_CLICK:
                    g_params.ch1_enabled = !g_params.ch1_enabled;           // 短按切换使能
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;                                     // 长按进入选择模式
                    break;
                default: break;
            }
            break;

        /* ── MODE_CH2: CH2 单通道模式 ──
         * 逻辑与 CH1 完全对称, 仅操作目标为 CH2 参数
         */
        case MODE_CH2:
            switch (ev) {
                case EVENT_CW:
                    set_ch_cursor_value(2, get_ch_cursor_value(2) + 1);    // CH2 参数 +1
                    break;
                case EVENT_CCW: {
                    u32 v = get_ch_cursor_value(2);
                    if (v > 0) set_ch_cursor_value(2, v - 1);              // CH2 参数 -1
                    break;
                }
                case EVENT_CLICK:
                    g_params.ch2_enabled = !g_params.ch2_enabled;           // 短按切换使能
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        /* ── MODE_TEST: 自动测试模式 ──
         * 7 个参数项: 通道/频率/占空比/循环次数/ON时间/OFF时间/START
         * 旋钮: 调节当前光标项的值 (使用 set_test_cursor_value, 支持有符号增量)
         * 短按: 光标在 START 项时启动测试
         * 长按: 进入选择模式
         */
        case MODE_TEST:
            switch (ev) {
                case EVENT_CW:
                    set_test_cursor_value(1);                          // 顺时针: +1
                    break;
                case EVENT_CCW:
                    set_test_cursor_value(-1);                         // 逆时针: -1
                    break;
                case EVENT_CLICK:
                    // 光标在 START 项时启动测试
                    if (g_menu.cursor == TEST_ITEM_START) {
                        test_running = 1;                              // 置运行标志
                        test_record_count = 0;                         // 清空记录
                        export_index = 0;                              // 重置导出
                    }
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;                               // 长按进入选择模式
                    break;
                default: break;
            }
            break;

        /* ── MODE_VSP: VSP 模拟电压输出 ──
         * 2 个参数项: 输出电压 (0~500 = 0.00~5.00V) / 使能开关
         * 旋钮: 调节电压值 (0.01V 步进, 存储值×100)
         * 短按: 切换 VSP 使能
         * 长按: 进入选择模式
         */
        case MODE_VSP:
            switch (ev) {
                case EVENT_CW:
                    if (g_menu.cursor == VSP_ITEM_VOLTAGE) {
                        if (g_params.vsp_voltage_x100 < 500)
                            g_params.vsp_voltage_x100++;    // 电压 +0.01V
                    }
                    break;
                case EVENT_CCW:
                    if (g_menu.cursor == VSP_ITEM_VOLTAGE) {
                        if (g_params.vsp_voltage_x100 > 0)
                            g_params.vsp_voltage_x100--;    // 电压 -0.01V
                    }
                    break;
                case EVENT_CLICK:
                    g_params.vsp_enabled = !g_params.vsp_enabled;  // 短按切换使能
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;                           // 长按进入选择模式
                    break;
                default: break;
            }
            break;

        default: break;
    }
}
