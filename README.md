# Yorush

Yorush is a lightweight command shell parser for embedded projects.

It is distributed as a single header, does not depend on `malloc`, and does not require `printf` or a full terminal framework. The goal is to keep the integration path short and the runtime behavior easy to reason about, while still covering the shell features that are often enough for MCU-side debugging and simple command interaction, such as command dispatch, argv-style tokenization, built-in help output, optional command prefixes, and byte-by-byte UART feeding.

It is a good fit for STM32 or other small embedded projects that want a tiny interactive command layer without pulling in a larger console stack.

---

> **Yoru Series**
>
> A family of lightweight utility libraries for STM32 HAL. Each library can be used independently or combined as needed.
>
> | Library | Role |
> | --- | --- |
> | [Yorulog](https://github.com/ExMikuPro/Yorulog) | Lightweight UART logger |
> | [Yorush](https://github.com/ExMikuPro/Yorush) | Lightweight UART shell / command parser |
> | [Yorunvm](https://github.com/ExMikuPro/Yorunvm) | STM32 on-chip NVM / Flash access helper |
> | [Yorukv](https://github.com/ExMikuPro/Yorukv) | Lightweight KV configuration library |
> | [Yorubench](https://github.com/ExMikuPro/Yorubench) | Lightweight performance measurement library |
> | [Yoruassert](https://github.com/ExMikuPro/Yoruassert.git) | Lightweight assertion helper |

---

## Key Features

* Single header: just include `yorush.h`
* No dynamic allocation
* No `printf`
* No stdlib tokenizer dependency
* Command table based dispatch
* Built-in `help` command output
* Supports argv-style tokenization from a single input line
* Supports feeding input one byte at a time or as a full buffer
* Supports optional command prefixes:

  * `""` for plain commands
  * `"/"` for slash-style commands
  * `"AT+"` for modem-style commands
* Supports optional local echo
* Supports pluggable output callbacks with `YORUSH_SetWrite()` / `YORUSH_SetWriteLine()`
* Can optionally bind to Yorulog for shell output

---

## Why use a tiny shell

On small MCUs, a shell is often useful long before a full CLI framework is justified.

In many projects, the actual need is modest: receive a line from UART, split it into tokens, look up a command, and print a few short responses. A larger console framework may add abstractions that are helpful on bigger systems, but unnecessary for firmware that only needs a few debug or maintenance commands.

Yorush keeps that path small and direct. The command model is intentionally simple, so it is easy to drop into an existing project and easy to audit when debugging input behavior.

---

## Quick Start

### 1. Add the header

Place `yorush.h` in your project, for example:

```text
/Core/Yorush/yorush.h
```

### 2. Define a command table

```c
static void cmd_ping(int argc, char **argv)
{
    (void)argc;
    (void)argv;
}

static const YORUSH_CommandTypeDef shell_cmds[] = {
    { "ping", cmd_ping, "reply with pong" },
};
```

### 3. Initialize the shell

```c
static YORUSH_HandleTypeDef hShell;

YORUSH_Init(&hShell, shell_cmds, (yorush_u16_t)YORUSH_ARRAY_SIZE(shell_cmds));
```

### 4. Feed incoming bytes

```c
YORUSH_InputByte(&hShell, rx_byte);
```

When Yorush receives `\r` or `\n`, it terminates the current line and executes the command.

---

## Basic Usage

```c
static void shell_write(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

static void cmd_ping(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_write("pong\r\n");
}

static const YORUSH_CommandTypeDef shell_cmds[] = {
    { "ping", cmd_ping, "reply with pong" },
};

static YORUSH_HandleTypeDef hShell;

void app_shell_init(void)
{
    YORUSH_Init(&hShell, shell_cmds, (yorush_u16_t)YORUSH_ARRAY_SIZE(shell_cmds));
    YORUSH_SetWrite(&hShell, shell_write);
}

void app_shell_rx_byte(uint8_t ch)
{
    YORUSH_InputByte(&hShell, ch);
}
```

If you also provide `YORUSH_SetWriteLine()`, the built-in help output can emit complete lines without splitting them into multiple raw writes.

---

## Input Model

Yorush stores the current line in an internal buffer and tokenizes it in place when a command is executed.

That means:

* input is line-oriented
* spaces and tabs are used as token separators
* the original input line is modified during tokenization
* the command handler receives classic `argc` / `argv` style arguments

This model is deliberately simple and works well for short UART commands such as:

```text
ping
add 1 2 3
level trace
```

Quoted strings, escape sequences, and advanced shell grammar are intentionally out of scope.

---

## Built-in Help

Yorush reserves `help` as a built-in command.

Calling `help` prints:

* a `Commands:` header
* one line per command
* the command name and help text when available

Example output:

```text
Commands:
  ping - reply with pong
  info - show shell info
```

If a command is not found, Yorush prints:

```text
Unknown command: <name>
```

---

## Command Prefix Filtering

If `YORUSH_CMD_PREFIX` is not empty, Yorush only accepts input lines that start with that prefix.

For example:

```c
#define YORUSH_CMD_PREFIX "/"
#include "yorush.h"
```

With that configuration:

* `/ping` is accepted
* `ping` is ignored

This is useful when the same UART channel carries both shell input and other plain text, or when you want the command format to be visually distinct.

---

## Output Options

### Custom write callbacks

By default, Yorush does not know how to print output.

You can provide:

```c
YORUSH_SetWrite(&hShell, shell_write);
YORUSH_SetWriteLine(&hShell, shell_writeln);
```

`YORUSH_SetWrite()` is used for raw text output.
`YORUSH_SetWriteLine()` is preferred for full-line helper output such as `help`.

If `WriteLine` is not set, Yorush falls back to `Write` plus the configured newline mode.

---

### Bind to Yorulog

If the project already uses Yorulog, Yorush can reuse it directly for shell output:

```c
#define YORUSH_USE_YORULOG 1
#include "yorush.h"
```

In that mode:

* raw helper output goes through `YORULOG_Print()`
* line output goes through `YORULOG_PrintLongln()`

This is a practical setup when the shell and the logger share the same UART path.

---

## API

### Initialize

```c
YORUSH_Init(&hShell, shell_cmds, (yorush_u16_t)YORUSH_ARRAY_SIZE(shell_cmds));
```

This resets the line buffer, clears argv storage, installs the command table, and applies the default echo setting from `YORUSH_ECHO`.

---

### Feed input

```c
YORUSH_InputByte(&hShell, ch);
YORUSH_InputBuffer(&hShell, buf, len);
```

Use `YORUSH_InputByte()` for interrupt-driven or polling UART paths.
Use `YORUSH_InputBuffer()` when a full command string or buffered input chunk is already available.

---

### Execute a line directly

```c
YORUSH_ExecuteLine(&hShell, line);
```

This is useful for tests, scripted input, or command replay.

The line buffer is tokenized in place, so the input string must be writable.

---

### Set output behavior

```c
YORUSH_SetWrite(&hShell, shell_write);
YORUSH_SetWriteLine(&hShell, shell_writeln);
YORUSH_SetEcho(&hShell, 1u);
```

`YORUSH_SetEcho()` controls whether incoming bytes are echoed while typing.

---

### Print the command list

```c
YORUSH_PrintHelp(&hShell);
```

This prints the same built-in command listing that the `help` command uses internally.

---

## Configuration Macros

All configuration macros must be defined before including `yorush.h`.

| Macro | Default | Description |
| --- | ---: | --- |
| `YORUSH_ENABLE` | `1` | master enable switch |
| `YORUSH_LINE_BUF_SIZE` | `1024` | input line buffer size, including trailing `\0` |
| `YORUSH_ARGV_MAX` | `8` | maximum argv entries produced by tokenization |
| `YORUSH_ECHO` | `0` | whether input bytes are echoed by default |
| `YORUSH_CRLF` | `1` | whether helper output uses `\r\n` |
| `YORUSH_CMD_PREFIX` | `""` | optional command prefix filter |
| `YORUSH_LOCK()` | empty | lock hook for interrupt-safe feeding |
| `YORUSH_UNLOCK()` | empty | unlock hook for interrupt-safe feeding |
| `YORUSH_USE_YORULOG` | `0` | route helper output through Yorulog |
| `YORUSH_YORULOG_HEADER` | `"../Yorulog/yorulog.h"` | custom Yorulog include path |

---

## Behavior Notes

### Newline handling

Both `\r` and `\n` are accepted as line terminators.

If the input stream uses `\r\n`, Yorush avoids executing the same command twice by suppressing the second half of the pair.

---

### Backspace handling

Backspace (`\b`) and DEL (`0x7F`) remove the last buffered character when possible.

This is enough for simple serial terminals, but Yorush does not implement a full line editor.

---

### Buffer limits

If the current line reaches `YORUSH_LINE_BUF_SIZE - 1`, extra bytes are silently ignored until the line is terminated.

If tokenization reaches `YORUSH_ARGV_MAX`, additional arguments are not stored.

For small debug shells, that tradeoff is usually acceptable and keeps the implementation straightforward.

---

## Suitable Use Cases

Yorush works well for:

* STM32 HAL projects that want a tiny UART command layer
* board bring-up and factory debug commands
* simple parameter inspection or runtime toggles
* projects already using Yorulog and wanting shell output on the same channel
* test or demo firmware that needs a few human-readable commands

It is less suitable for:

* projects that need quoted strings or complex parsing rules
* multi-session consoles with permissions or history
* shells that require tab completion or cursor editing
* large CLI frameworks with nested command trees

---

## Project Positioning

Yorush is not a full terminal framework.

It is closer to a tiny command interpreter that is easy to embed directly into firmware: small, predictable, and oriented around the common embedded pattern of "receive a line from UART, split it, and run a handler."

---

## License

This project is licensed under the MIT License.

See the [LICENSE](./LICENSE) file for details.
