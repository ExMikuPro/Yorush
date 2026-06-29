#ifndef YORUSH_H
#define YORUSH_H

/* =========================================================
 *  Optional Yorulog binding
 * ========================================================= */
#ifndef YORUSH_USE_YORULOG
#define YORUSH_USE_YORULOG 0
#endif

#if YORUSH_USE_YORULOG
  #ifndef YORUSH_YORULOG_HEADER
    #define YORUSH_YORULOG_HEADER "../Yorulog/yorulog.h"
  #endif
  #include YORUSH_YORULOG_HEADER
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 *  Minimal Integer Types (no stdint/stddef dependency)
 * ========================================================= */

typedef
#if defined(__UINT8_TYPE__)
__UINT8_TYPE__
#else
unsigned char
#endif
yorush_u8_t;

typedef
#if defined(__UINT16_TYPE__)
__UINT16_TYPE__
#else
unsigned short
#endif
yorush_u16_t;

/* =========================================================
 *  User Configuration
 * ========================================================= */

/* Master switch: 1 = enabled, 0 = disabled */
#ifndef YORUSH_ENABLE
#define YORUSH_ENABLE 1
#endif

/* Maximum input line length, including the trailing '\0' */
#ifndef YORUSH_LINE_BUF_SIZE
#define YORUSH_LINE_BUF_SIZE 1024u
#endif

/* Maximum argv entries produced by the tokenizer */
#ifndef YORUSH_ARGV_MAX
#define YORUSH_ARGV_MAX 8u
#endif

/* Enable local echo while feeding input */
#ifndef YORUSH_ECHO
#define YORUSH_ECHO 0
#endif

/* Built-in newline mode for helper output */
#ifndef YORUSH_CRLF
#define YORUSH_CRLF 1
#endif

/* Optional command prefix string.
 * Examples:
 *   #define YORUSH_CMD_PREFIX "/"
 *   #define YORUSH_CMD_PREFIX "AT+"
 * Default "" means no prefix filtering.
 */
#ifndef YORUSH_CMD_PREFIX
#define YORUSH_CMD_PREFIX ""
#endif

/* Optional lock hooks, useful when feeding from interrupt context */
#ifndef YORUSH_LOCK
#define YORUSH_LOCK()   do{}while(0)
#endif

#ifndef YORUSH_UNLOCK
#define YORUSH_UNLOCK() do{}while(0)
#endif

/* =========================================================
 *  Type Definitions
 * ========================================================= */

typedef void (*YORUSH_CommandHandlerTypeDef)(int argc, char **argv);
typedef void (*YORUSH_WriteFnTypeDef)(const char *s);

typedef struct {
    const char *Name;
    YORUSH_CommandHandlerTypeDef Handler;
    const char *Help;
} YORUSH_CommandTypeDef;

typedef struct {
    const YORUSH_CommandTypeDef *CmdTable;
    yorush_u16_t CmdCount;
    yorush_u16_t LineLen;
    yorush_u8_t LastWasCR;
    yorush_u8_t Echo;
    YORUSH_WriteFnTypeDef Write;
    YORUSH_WriteFnTypeDef WriteLine;
    char LineBuf[YORUSH_LINE_BUF_SIZE];
    char *Argv[YORUSH_ARGV_MAX];
} YORUSH_HandleTypeDef;

#ifndef YORUSH_ARRAY_SIZE
#define YORUSH_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* =========================================================
 *  Internal Helpers
 * ========================================================= */

static inline unsigned yorush__is_space_(char c)
{
    return (unsigned)((c == ' ') || (c == '\t'));
}

static inline unsigned yorush__streq_(const char *a, const char *b)
{
    if (!a || !b) return 0u;
    while ((*a != '\0') && (*b != '\0')) {
        if (*a != *b) return 0u;
        ++a;
        ++b;
    }
    return (unsigned)(*a == *b);
}

static inline unsigned yorush__starts_with_(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0u;

    while (*prefix != '\0') {
        if (*s != *prefix) {
            return 0u;
        }
        ++s;
        ++prefix;
    }

    return 1u;
}

static inline char *yorush__skip_prefix_(char *s, const char *prefix)
{
    if (!s || !prefix) return s;

    while (*prefix != '\0') {
        ++s;
        ++prefix;
    }

    return s;
}

static inline void yorush__write_(YORUSH_HandleTypeDef *hsh, const char *s)
{
#if YORUSH_ENABLE
    if (!s) return;
#if YORUSH_USE_YORULOG
    (void)hsh;
    YORULOG_Print(s);
#else
    if (hsh && hsh->Write) hsh->Write(s);
#endif
#else
    (void)hsh;
    (void)s;
#endif
}

static inline void yorush__writeln_(YORUSH_HandleTypeDef *hsh, const char *s)
{
#if YORUSH_ENABLE
#if YORUSH_USE_YORULOG
    (void)hsh;
    YORULOG_PrintLongln(s ? s : "");
#else
    if (hsh && hsh->WriteLine) {
        hsh->WriteLine(s ? s : "");
    } else {
        yorush__write_(hsh, s ? s : "");
#if YORUSH_CRLF
        yorush__write_(hsh, "\r\n");
#else
        yorush__write_(hsh, "\n");
#endif
    }
#endif
#else
    (void)hsh;
    (void)s;
#endif
}

static inline int yorush__tokenize_(YORUSH_HandleTypeDef *hsh, char *line)
{
    int argc = 0;
    char *p = line;

    if (!hsh || !line) return 0;

    while (*p != '\0') {
        while (yorush__is_space_(*p)) {
            *p++ = '\0';
        }
        if (*p == '\0') break;

        if (argc >= (int)YORUSH_ARGV_MAX) {
            return argc;
        }

        hsh->Argv[argc++] = p;

        while ((*p != '\0') && !yorush__is_space_(*p)) {
            ++p;
        }
    }

    return argc;
}

static inline const YORUSH_CommandTypeDef *yorush__find_cmd_(const YORUSH_HandleTypeDef *hsh, const char *name)
{
    yorush_u16_t i;

    if (!hsh || !name) return (const YORUSH_CommandTypeDef *)0;

    for (i = 0u; i < hsh->CmdCount; ++i) {
        const YORUSH_CommandTypeDef *cmd = &hsh->CmdTable[i];
        if (yorush__streq_(cmd->Name, name)) {
            return cmd;
        }
    }

    return (const YORUSH_CommandTypeDef *)0;
}

static inline void yorush__append_str_(char *dst, yorush_u16_t *len, yorush_u16_t max_len, const char *src)
{
    if (!dst || !len || (max_len == 0u)) return;

    if (!src) {
        src = "";
    }

    while ((*src != '\0') && (*len + 1u < max_len)) {
        dst[*len] = *src;
        ++(*len);
        ++src;
    }

    dst[*len] = '\0';
}

/* =========================================================
 *  Public API
 * ========================================================= */

static inline void YORUSH_Init(YORUSH_HandleTypeDef *hsh,
                               const YORUSH_CommandTypeDef *cmd_table,
                               yorush_u16_t cmd_count)
{
    yorush_u16_t i;

    if (!hsh) return;

    hsh->CmdTable = cmd_table;
    hsh->CmdCount = cmd_count;
    hsh->LineLen = 0u;
    hsh->LastWasCR = 0u;
    hsh->Echo = (yorush_u8_t)YORUSH_ECHO;
    hsh->Write = (YORUSH_WriteFnTypeDef)0;
    hsh->WriteLine = (YORUSH_WriteFnTypeDef)0;
    hsh->LineBuf[0] = '\0';

    for (i = 0u; i < (yorush_u16_t)YORUSH_ARGV_MAX; ++i) {
        hsh->Argv[i] = (char *)0;
    }
}

static inline void YORUSH_SetWrite(YORUSH_HandleTypeDef *hsh, YORUSH_WriteFnTypeDef fn)
{
    if (!hsh) return;
    hsh->Write = fn;
}

static inline void YORUSH_SetWriteLine(YORUSH_HandleTypeDef *hsh, YORUSH_WriteFnTypeDef fn)
{
    if (!hsh) return;
    hsh->WriteLine = fn;
}

static inline void YORUSH_SetEcho(YORUSH_HandleTypeDef *hsh, yorush_u8_t enable)
{
    if (!hsh) return;
    hsh->Echo = (yorush_u8_t)(enable ? 1u : 0u);
}

static inline void YORUSH_PrintHelp(YORUSH_HandleTypeDef *hsh)
{
#if YORUSH_ENABLE
    yorush_u16_t i;
    char line[YORUSH_LINE_BUF_SIZE];

    if (!hsh) return;

    yorush__writeln_(hsh, "Commands:");
    for (i = 0u; i < hsh->CmdCount; ++i) {
        const YORUSH_CommandTypeDef *cmd = &hsh->CmdTable[i];
        yorush_u16_t len = 0u;

        line[0] = '\0';
        yorush__append_str_(line, &len, (yorush_u16_t)sizeof(line), "  ");
        yorush__append_str_(line, &len, (yorush_u16_t)sizeof(line), cmd->Name ? cmd->Name : "(null)");
        if (cmd->Help && (*cmd->Help != '\0')) {
            yorush__append_str_(line, &len, (yorush_u16_t)sizeof(line), " - ");
            yorush__append_str_(line, &len, (yorush_u16_t)sizeof(line), cmd->Help);
        }
        yorush__writeln_(hsh, line);
    }
#else
    (void)hsh;
#endif
}

static inline void YORUSH_ExecuteLine(YORUSH_HandleTypeDef *hsh, char *line)
{
#if YORUSH_ENABLE
    int argc;
    const YORUSH_CommandTypeDef *cmd;
    char *cmd_line = line;

    if (!hsh || !line) return;

    if (YORUSH_CMD_PREFIX[0] != '\0') {
        if (!yorush__starts_with_(line, YORUSH_CMD_PREFIX)) {
            return;
        }

        cmd_line = yorush__skip_prefix_(line, YORUSH_CMD_PREFIX);
    }

    if (*cmd_line == '\0') return;

    argc = yorush__tokenize_(hsh, cmd_line);
    if (argc <= 0) return;

    cmd = yorush__find_cmd_(hsh, hsh->Argv[0]);
    if (cmd && cmd->Handler) {
        cmd->Handler(argc, hsh->Argv);
        return;
    }

    if (yorush__streq_(hsh->Argv[0], "help")) {
        YORUSH_PrintHelp(hsh);
        return;
    }

    yorush__write_(hsh, "Unknown command: ");
    yorush__writeln_(hsh, hsh->Argv[0]);
#else
    (void)hsh;
    (void)line;
#endif
}

static inline void YORUSH_InputByte(YORUSH_HandleTypeDef *hsh, yorush_u8_t ch)
{
#if YORUSH_ENABLE
    if (!hsh) return;

    YORUSH_LOCK();

    if (hsh->Echo) {
        char echo_buf[2];
        echo_buf[0] = (char)ch;
        echo_buf[1] = '\0';
        yorush__write_(hsh, echo_buf);
    }

    if ((ch == '\r') || (ch == '\n')) {
        if ((ch == '\n') && hsh->LastWasCR) {
            hsh->LastWasCR = 0u;
            YORUSH_UNLOCK();
            return;
        }

        hsh->LastWasCR = (yorush_u8_t)(ch == '\r');
        hsh->LineBuf[hsh->LineLen] = '\0';
        YORUSH_ExecuteLine(hsh, hsh->LineBuf);
        hsh->LineLen = 0u;
        hsh->LineBuf[0] = '\0';
        YORUSH_UNLOCK();
        return;
    }

    hsh->LastWasCR = 0u;

    if ((ch == '\b') || (ch == 0x7Fu)) {
        if (hsh->LineLen > 0u) {
            --hsh->LineLen;
            hsh->LineBuf[hsh->LineLen] = '\0';
        }
        YORUSH_UNLOCK();
        return;
    }

    if (hsh->LineLen < (yorush_u16_t)(YORUSH_LINE_BUF_SIZE - 1u)) {
        hsh->LineBuf[hsh->LineLen++] = (char)ch;
        hsh->LineBuf[hsh->LineLen] = '\0';
    }

    YORUSH_UNLOCK();
#else
    (void)hsh;
    (void)ch;
#endif
}

static inline void YORUSH_InputBuffer(YORUSH_HandleTypeDef *hsh, const yorush_u8_t *buf, yorush_u16_t len)
{
#if YORUSH_ENABLE
    yorush_u16_t i;

    if (!hsh || !buf) return;

    for (i = 0u; i < len; ++i) {
        YORUSH_InputByte(hsh, buf[i]);
    }
#else
    (void)hsh;
    (void)buf;
    (void)len;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* YORUSH_H */
