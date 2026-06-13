#pragma once
#include "../stdint.h"
#include "../drivers/keyboard/keyboard.h"
#include "../ui/window.h"

#define SHELL_W          560
#define SHELL_H          340
#define SHELL_DEFAULT_X  80
#define SHELL_DEFAULT_Y  60
#define SHELL_INPUT_H    18
#define SHELL_PADDING    8
#define SHELL_LINE_H     11
#define SHELL_MAX_LINES  128
#define SHELL_LINE_MAX   80
#define SHELL_HIST_MAX   32
#define SHELL_HIST_LEN   80
#define SHELL_INPUT_MAX  79

#define COL_SHELL_BG          0x080c10
#define COL_SHELL_INPUT_BG    0x0c1018
#define COL_SHELL_INPUT_BD    0x2a4a6a
#define COL_SHELL_PROMPT      0x4a9aca
#define COL_SHELL_TEXT        0xc8d8e8
#define COL_SHELL_TEXT_DIM    0x506070
#define COL_SHELL_TEXT_ERR    0xe06060
#define COL_SHELL_TEXT_OK     0x60c080
#define COL_SHELL_TEXT_DIR    0x7ab8e8
#define COL_SHELL_CURSOR      0x80c0ff
#define COL_SHELL_SEL_BG      0x1e3a5f

typedef struct {
    char lines[SHELL_MAX_LINES][SHELL_LINE_MAX];
    uint32_t line_colors[SHELL_MAX_LINES];
    int line_head;
    int line_count;
    char input[SHELL_INPUT_MAX + 1];
    int input_len;
    char history[SHELL_HIST_MAX][SHELL_HIST_LEN];
    int hist_count;
    int hist_cursor;
    int cwd_depth;
    int cwd_idx[8];
    char cwd_name[8][32];
    int scroll;
    int cursor_blink;
} shell_t;

shell_t *shell_create(void);
void shell_destroy(shell_t *sh);
void shell_draw(shell_t *sh, window_t *win, int active);
int shell_handle_key(shell_t *sh, window_t *win, const key_event_t *evt);