# Yorush

Yorush 是一个面向嵌入式项目的轻量级命令 shell 解析器。

它只有一个头文件，不依赖 `malloc`，也不要求引入 `printf` 或完整的终端框架。设计目标是尽量缩短接入路径、保持运行行为简单可控，同时覆盖 MCU 侧调试和基础命令交互里常见的 shell 能力，例如命令分发、argv 风格分词、内建帮助输出、可选命令前缀，以及按字节喂入的 UART 输入处理。

适合用在 STM32 或其他小型嵌入式项目中，在不引入更重 CLI 框架的前提下，提供一个足够小巧的交互命令层。

---

> **Yoru 系列**
>
> 一组面向 STM32 HAL 的轻量级工具库。各库可独立使用，也可以组合使用。
>
> | 库 | 定位 |
> | --- | --- |
> | [Yorulog](https://github.com/ExMikuPro/Yorulog) | 轻量级 UART 日志库 |
> | [Yorush](https://github.com/ExMikuPro/Yorush) | 轻量级 UART Shell / 命令解析器 |
> | [Yorunvm](https://github.com/ExMikuPro/Yorunvm) | STM32 片上 NVM / Flash / EEPROM 访问辅助库 |
> | [Yorukv](https://github.com/ExMikuPro/Yorukv) | 轻量级 KV 配置库 |
> | [Yorubench](https://github.com/ExMikuPro/Yorubench) | 轻量级性能测量库 |
> | [Yoruassert](https://github.com/ExMikuPro/Yoruassert.git) | 轻量级断言辅助库 |

---

## 主要特性

* 单头文件：只需要引入 `yorush.h`
* 不使用动态内存分配
* 不使用 `printf`
* 不依赖标准库分词器
* 基于命令表进行分发
* 内建 `help` 命令输出
* 支持从单行输入中做 argv 风格分词
* 支持按字节喂入输入，也支持一次喂入整段缓冲区
* 支持可选命令前缀：

  * `""`：普通命令
  * `"/"`：斜杠风格命令
  * `"AT+"`：调制解调器风格命令
* 支持可选本地回显
* 支持通过 `YORUSH_SetWrite()` / `YORUSH_SetWriteLine()` 挂接输出回调
* 可选绑定 Yorulog 作为 shell 输出后端

---

## 为什么需要一个小 shell

在小型 MCU 上，很多时候真正需要的并不是一个完整 CLI 框架，而只是一个“够用”的命令入口。

不少项目的需求其实很直接：从 UART 收一行文本，按空白拆成参数，查找命令表，然后回几行简短结果。更大的 console 框架在复杂系统里当然有价值，但对于只需要少量调试命令的固件来说，往往会引入不必要的抽象和额外体积。

Yorush 选择把这条路径保持得尽量短。命令模型有意做得简单，既方便直接塞进现有工程，也更容易在调试串口输入问题时快速看清行为。

---

## 快速开始

### 1. 添加头文件

把 `yorush.h` 放到你的工程目录中，例如：

```text
/Core/Yorush/yorush.h
```

### 2. 定义命令表

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

### 3. 初始化 shell

```c
static YORUSH_HandleTypeDef hShell;

YORUSH_Init(&hShell, shell_cmds, (yorush_u16_t)YORUSH_ARRAY_SIZE(shell_cmds));
```

### 4. 喂入输入字节

```c
YORUSH_InputByte(&hShell, rx_byte);
```

当 Yorush 收到 `\r` 或 `\n` 时，会结束当前行并执行命令。

---

## 基本用法

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

如果同时提供 `YORUSH_SetWriteLine()`，内建帮助输出就可以按“整行”发出，而不会被拆成多次原始写出。

---

## 输入模型

Yorush 会把当前输入行保存在内部缓冲区中，并在执行命令时原地完成分词。

这意味着：

* 输入是按“行”处理的
* 空格和制表符会被当作参数分隔符
* 原始输入字符串会在分词过程中被修改
* 命令处理函数拿到的是传统 `argc` / `argv` 风格参数

这种模型刻意保持简单，很适合下面这类短串口命令：

```text
ping
add 1 2 3
level trace
```

带引号字符串、转义序列以及更复杂的 shell 语法，不在当前设计范围内。

---

## 内建帮助

Yorush 预留了 `help` 作为内建命令。

调用 `help` 时会输出：

* 一行 `Commands:` 标题
* 每个命令各占一行
* 如果提供了帮助文本，则输出“命令名 + 帮助说明”

输出示例：

```text
Commands:
  ping - reply with pong
  info - show shell info
```

如果命令不存在，Yorush 会输出：

```text
Unknown command: <name>
```

---

## 命令前缀过滤

如果 `YORUSH_CMD_PREFIX` 不是空字符串，Yorush 只会接受带此前缀的输入行。

例如：

```c
#define YORUSH_CMD_PREFIX "/"
#include "yorush.h"
```

在这个配置下：

* `/ping` 会被接受
* `ping` 会被忽略

这在同一个 UART 通道里既混有 shell 输入、又混有其他普通文本时很有用；如果希望命令格式更醒目，也可以这样做。

---

## 输出方式

### 自定义写出回调

默认情况下，Yorush 并不知道应该如何输出文本。

你可以提供：

```c
YORUSH_SetWrite(&hShell, shell_write);
YORUSH_SetWriteLine(&hShell, shell_writeln);
```

`YORUSH_SetWrite()` 用于原始文本输出。
`YORUSH_SetWriteLine()` 更适合 `help` 这类完整行输出。

如果没有设置 `WriteLine`，Yorush 会退回到 `Write`，再根据换行配置补上行尾。

---

### 绑定 Yorulog

如果工程里已经在使用 Yorulog，Yorush 可以直接复用它作为 shell 输出通道：

```c
#define YORUSH_USE_YORULOG 1
#include "yorush.h"
```

在这种模式下：

* 原始帮助输出会走 `YORULOG_Print()`
* 按行输出会走 `YORULOG_PrintLongln()`

当 shell 和 logger 共用同一条 UART 输出路径时，这通常是比较顺手的接法。

---

## API

### 初始化

```c
YORUSH_Init(&hShell, shell_cmds, (yorush_u16_t)YORUSH_ARRAY_SIZE(shell_cmds));
```

这个调用会重置行缓冲区、清空 argv 存储、安装命令表，并应用 `YORUSH_ECHO` 指定的默认回显设置。

---

### 喂入输入

```c
YORUSH_InputByte(&hShell, ch);
YORUSH_InputBuffer(&hShell, buf, len);
```

`YORUSH_InputByte()` 适合中断驱动或轮询式 UART 输入路径。
`YORUSH_InputBuffer()` 适合已经拿到整段命令串或缓冲数据块的场景。

---

### 直接执行一行命令

```c
YORUSH_ExecuteLine(&hShell, line);
```

这对测试、脚本化输入或命令回放很有用。

由于它会原地分词，所以传入的字符串必须是可写的。

---

### 设置输出行为

```c
YORUSH_SetWrite(&hShell, shell_write);
YORUSH_SetWriteLine(&hShell, shell_writeln);
YORUSH_SetEcho(&hShell, 1u);
```

`YORUSH_SetEcho()` 用来控制输入字节在输入时是否回显。

---

### 输出命令列表

```c
YORUSH_PrintHelp(&hShell);
```

它会输出与内建 `help` 命令相同的命令列表。

---

## 配置宏

所有配置宏都需要在包含 `yorush.h` 之前定义。

| 宏 | 默认值 | 说明 |
| --- | ---: | --- |
| `YORUSH_ENABLE` | `1` | 总开关 |
| `YORUSH_LINE_BUF_SIZE` | `1024` | 输入行缓冲区大小，包含结尾 `\0` |
| `YORUSH_ARGV_MAX` | `8` | 分词后最多保留的 argv 项数 |
| `YORUSH_ECHO` | `0` | 默认是否开启输入回显 |
| `YORUSH_CRLF` | `1` | 辅助输出是否使用 `\r\n` |
| `YORUSH_CMD_PREFIX` | `""` | 可选命令前缀过滤 |
| `YORUSH_LOCK()` | 空实现 | 中断安全喂入时可挂接的加锁钩子 |
| `YORUSH_UNLOCK()` | 空实现 | 中断安全喂入时可挂接的解锁钩子 |
| `YORUSH_USE_YORULOG` | `0` | 是否通过 Yorulog 输出辅助文本 |
| `YORUSH_YORULOG_HEADER` | `"../Yorulog/yorulog.h"` | 自定义 Yorulog 头文件路径 |

---

## 行为说明

### 换行处理

Yorush 同时接受 `\r` 和 `\n` 作为行结束符。

如果输入流使用 `\r\n`，Yorush 会抑制后半个换行，从而避免同一条命令被执行两次。

---

### 退格处理

退格（`\b`）和 DEL（`0x7F`）会在可能的情况下删除最后一个已缓冲字符。

这足够覆盖简单串口终端的输入习惯，但 Yorush 并不提供完整的行编辑器能力。

---

### 缓冲区限制

如果当前输入行长度达到 `YORUSH_LINE_BUF_SIZE - 1`，后续字节会被静默忽略，直到这一行结束。

如果参数分词数量达到 `YORUSH_ARGV_MAX`，额外参数不会继续保存。

对于小型调试 shell 来说，这个取舍通常是可以接受的，同时也让实现保持简单直接。

---

## 适用场景

Yorush 适合以下场景：

* 需要一个小型 UART 命令层的 STM32 HAL 项目
* 板级 bring-up 或产线调试命令
* 简单参数查看、运行期开关或诊断入口
* 已经使用 Yorulog，并希望 shell 输出走同一通道的工程
* 只需要少量人工交互命令的测试或演示固件

不太适合以下场景：

* 需要支持带引号字符串或复杂语法的项目
* 需要多会话、权限控制或历史记录的控制台
* 需要 Tab 补全、光标移动或完整行编辑的 shell
* 命令树复杂、层级很深的大型 CLI 框架

---

## 项目定位

Yorush 不是一个完整的终端框架。

它更像是一个适合直接嵌入固件的小命令解释器：体积小、行为可预测，围绕嵌入式里最常见的那条路径展开，也就是“从 UART 收一行，拆参数，然后调用处理函数”。

---

## 开源协议

本项目使用 MIT License 开源。

详细内容见 [LICENSE](./LICENSE) 文件。
