#include "shell.h"
#include "../ui/desktop.h"
#include "../ui/window.h"
#include "../fs/fs.h"
#include "../mm/alloc.h"
#include "../graphics/framebuffer.h"
#include "../graphics/text.h"

static int sh_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}
static void sh_strcpy(char *d, const char *s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}
static int sh_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static int sh_strncmp(const char *a, const char *b, int n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n < 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}
static void sh_memset(void *p, uint8_t v, int n) {
    uint8_t *b = (uint8_t *)p; while (n--) *b++ = v;
}
static void sh_itoa(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + n % 10; n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = 0;
}

static const char *sh_skip(const char *s) {
    while (*s == ' ') s++; return s;
}

static const char *sh_word(const char *s, char *dst, int max) {
    int i = 0;
    while (*s && *s != ' ' && i < max - 1) dst[i++] = *s++;
    dst[i] = 0;
    return sh_skip(s);
}

static void sh_puts(shell_t *sh, const char *text, uint32_t color) {
    int idx = sh->line_head % SHELL_MAX_LINES;
    sh_strcpy(sh->lines[idx], text, SHELL_LINE_MAX);
    sh->line_colors[idx] = color;
    sh->line_head++;
    if (sh->line_count < SHELL_MAX_LINES) sh->line_count++;
    sh->scroll = 0;
}

static void sh_puts_n(shell_t *sh, const char *text, int n, uint32_t color) {
    char buf[SHELL_LINE_MAX];
    int i = 0;
    while (i < n && i < SHELL_LINE_MAX - 1 && text[i]) {
        buf[i] = text[i]; i++;
    }
    buf[i] = 0;
    sh_puts(sh, buf, color);
}

static void sh_puts_ml(shell_t *sh, const char *text, uint32_t color) {
    char line[SHELL_LINE_MAX];
    int li = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n' || li >= SHELL_LINE_MAX - 1) {
            line[li] = 0;
            sh_puts(sh, line, color);
            li = 0;
            if (*p == '\n') { p++; continue; }
        }
        line[li++] = *p++;
    }
    if (li > 0) { line[li] = 0; sh_puts(sh, line, color); }
}

static int sh_cwd_parent(shell_t *sh) {
    if (sh->cwd_depth == 0) return VFS_ROOT_PARENT;
    return sh->cwd_idx[sh->cwd_depth - 1];
}

static void sh_build_prompt(shell_t *sh, char *buf, int max) {
    int i = 0;
    const char *pfx = "velox";
    const char *p = pfx; while (*p && i < max - 2) buf[i++] = *p++;
    buf[i++] = ':';
    if (sh->cwd_depth == 0) {
        buf[i++] = '/';
    } else {
        for (int d = 0; d < sh->cwd_depth && i < max - 2; d++) {
            buf[i++] = '/';
            const char *n = sh->cwd_name[d];
            while (*n && i < max - 2) buf[i++] = *n++;
        }
    }
    buf[i++] = '$';
    buf[i++] = ' ';
    buf[i] = 0;
}

static void cmd_help(shell_t *sh) {
    sh_puts(sh, "Built-in commands:", COL_SHELL_TEXT_OK);
    sh_puts(sh, "  help              show this message", COL_SHELL_TEXT);
    sh_puts(sh, "  ls                list directory", COL_SHELL_TEXT);
    sh_puts(sh, "  cd <dir>          change directory", COL_SHELL_TEXT);
    sh_puts(sh, "  pwd               print working dir", COL_SHELL_TEXT);
    sh_puts(sh, "  cat <file>        print file contents", COL_SHELL_TEXT);
    sh_puts(sh, "  echo <text>       print text", COL_SHELL_TEXT);
    sh_puts(sh, "  touch <name>      create empty file", COL_SHELL_TEXT);
    sh_puts(sh, "  mkdir <name>      create directory", COL_SHELL_TEXT);
    sh_puts(sh, "  rm <name>         delete file/dir", COL_SHELL_TEXT);
    sh_puts(sh, "  mv <old> <new>    rename file/dir", COL_SHELL_TEXT);
    sh_puts(sh, "  write <f> <text>  write text to file", COL_SHELL_TEXT);
    sh_puts(sh, "  clear             clear screen", COL_SHELL_TEXT);
    sh_puts(sh, "  ver               OS version info", COL_SHELL_TEXT);
}

static void cmd_ls(shell_t *sh, const char *arg) {
    int parent;
    if (*arg) {
        int tidx = vfs_find_in(arg, sh_cwd_parent(sh));
        if (tidx < 0) tidx = vfs_find(arg);
        if (tidx < 0 || !vfs.entries[tidx].is_dir) {
            sh_puts(sh, "ls: no such directory", COL_SHELL_TEXT_ERR);
            return;
        }
        parent = tidx;
    } else {
        parent = sh_cwd_parent(sh);
    }

    int found = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != parent) continue;
        if (!vfs.entries[i].is_dir) continue;
        char buf[SHELL_LINE_MAX];
        buf[0] = '['; buf[1] = 'd'; buf[2] = ']'; buf[3] = ' ';
        sh_strcpy(buf + 4, vfs.entries[i].name, SHELL_LINE_MAX - 4);
        sh_puts(sh, buf, COL_SHELL_TEXT_DIR);
        found++;
    }
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs.entries[i].used) continue;
        if ((int)vfs.entries[i].parent_idx != parent) continue;
        if (vfs.entries[i].is_dir) continue;
        char buf[SHELL_LINE_MAX];
        buf[0] = '['; buf[1] = 'f'; buf[2] = ']'; buf[3] = ' ';
        int ni = 4;
        const char *n = vfs.entries[i].name;
        while (*n && ni < 52) buf[ni++] = *n++;
        while (ni < 52) buf[ni++] = ' ';
        char sz[12]; sh_itoa(vfs.entries[i].size_bytes, sz);
        const char *sp = sz; while (*sp && ni < SHELL_LINE_MAX - 2) buf[ni++] = *sp++;
        buf[ni++] = 'B'; buf[ni] = 0;
        sh_puts(sh, buf, COL_SHELL_TEXT);
        found++;
    }
    if (!found) sh_puts(sh, "(empty)", COL_SHELL_TEXT_DIM);
}

static void cmd_pwd(shell_t *sh) {
    char buf[SHELL_LINE_MAX];
    int i = 0;
    if (sh->cwd_depth == 0) {
        buf[i++] = '/'; buf[i] = 0;
    } else {
        for (int d = 0; d < sh->cwd_depth && i < SHELL_LINE_MAX - 2; d++) {
            buf[i++] = '/';
            const char *n = sh->cwd_name[d];
            while (*n && i < SHELL_LINE_MAX - 1) buf[i++] = *n++;
        }
        buf[i] = 0;
    }
    sh_puts(sh, buf, COL_SHELL_TEXT);
}

static void cmd_cd(shell_t *sh, const char *arg) {
    if (!*arg || sh_strcmp(arg, "/") == 0) {
        sh->cwd_depth = 0;
        return;
    }
    if (sh_strcmp(arg, "..") == 0) {
        if (sh->cwd_depth > 0) sh->cwd_depth--;
        return;
    }
    int parent = sh_cwd_parent(sh);
    int tidx = vfs_find_in(arg, parent);
    if (tidx < 0) { sh_puts(sh, "cd: not found", COL_SHELL_TEXT_ERR); return; }
    if (!vfs.entries[tidx].is_dir) { sh_puts(sh, "cd: not a directory", COL_SHELL_TEXT_ERR); return; }
    if (sh->cwd_depth >= 8) { sh_puts(sh, "cd: too deep", COL_SHELL_TEXT_ERR); return; }
    sh->cwd_idx[sh->cwd_depth] = tidx;
    sh_strcpy(sh->cwd_name[sh->cwd_depth], arg, 32);
    sh->cwd_depth++;
}

static void cmd_cat(shell_t *sh, const char *arg) {
    if (!*arg) { sh_puts(sh, "usage: cat <file>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    int tidx = vfs_find_in(arg, parent);
    if (tidx < 0) tidx = vfs_find(arg);
    if (tidx < 0) { sh_puts(sh, "cat: file not found", COL_SHELL_TEXT_ERR); return; }
    if (vfs.entries[tidx].is_dir) { sh_puts(sh, "cat: is a directory", COL_SHELL_TEXT_ERR); return; }
    uint32_t sz = vfs.entries[tidx].size_bytes;
    if (sz == 0) { sh_puts(sh, "(empty file)", COL_SHELL_TEXT_DIM); return; }
    void *buf = mm_alloc(sz + 1);
    if (!buf) { sh_puts(sh, "cat: out of memory", COL_SHELL_TEXT_ERR); return; }
    uint32_t n = vfs_read(tidx, buf);
    ((char *)buf)[n] = 0;
    sh_puts_ml(sh, (char *)buf, COL_SHELL_TEXT);
    mm_free(buf);
}

static void cmd_echo(shell_t *sh, const char *arg) {
    sh_puts(sh, *arg ? arg : "", COL_SHELL_TEXT);
}

static void cmd_touch(shell_t *sh, const char *arg) {
    if (!*arg) { sh_puts(sh, "usage: touch <name>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    if (vfs_find_in(arg, parent) >= 0) { sh_puts(sh, "touch: already exists", COL_SHELL_TEXT_ERR); return; }
    int r = vfs_create_in(arg, 0, parent);
    if (r < 0) sh_puts(sh, "touch: failed", COL_SHELL_TEXT_ERR);
    else        sh_puts(sh, "created", COL_SHELL_TEXT_OK);
}

static void cmd_mkdir(shell_t *sh, const char *arg) {
    if (!*arg) { sh_puts(sh, "usage: mkdir <name>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    if (vfs_find_in(arg, parent) >= 0) { sh_puts(sh, "mkdir: already exists", COL_SHELL_TEXT_ERR); return; }
    int r = vfs_create_in(arg, 1, parent);
    if (r < 0) sh_puts(sh, "mkdir: failed", COL_SHELL_TEXT_ERR);
    else        sh_puts(sh, "created", COL_SHELL_TEXT_OK);
}

static void cmd_rm(shell_t *sh, const char *arg) {
    if (!*arg) { sh_puts(sh, "usage: rm <name>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    int tidx = vfs_find_in(arg, parent);
    if (tidx < 0) tidx = vfs_find(arg);
    if (tidx < 0) { sh_puts(sh, "rm: not found", COL_SHELL_TEXT_ERR); return; }
    vfs_delete(tidx);
    sh_puts(sh, "deleted", COL_SHELL_TEXT_OK);
}

static void cmd_mv(shell_t *sh, const char *arg) {
    char oldname[VFS_NAME_MAX], newname[VFS_NAME_MAX];
    const char *rest = sh_word(arg, oldname, VFS_NAME_MAX);
    sh_word(rest, newname, VFS_NAME_MAX);
    if (!*oldname || !*newname) { sh_puts(sh, "usage: mv <old> <new>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    int tidx = vfs_find_in(oldname, parent);
    if (tidx < 0) { sh_puts(sh, "mv: not found", COL_SHELL_TEXT_ERR); return; }
    if (vfs_find_in(newname, parent) >= 0) { sh_puts(sh, "mv: target exists", COL_SHELL_TEXT_ERR); return; }
    sh_strcpy(vfs.entries[tidx].name, newname, VFS_NAME_MAX);
    vfs_flush();
    sh_puts(sh, "renamed", COL_SHELL_TEXT_OK);
}

static void cmd_write(shell_t *sh, const char *arg) {
    char fname[VFS_NAME_MAX];
    const char *text = sh_word(arg, fname, VFS_NAME_MAX);
    if (!*fname || !*text) { sh_puts(sh, "usage: write <file> <text>", COL_SHELL_TEXT_ERR); return; }
    int parent = sh_cwd_parent(sh);
    int tidx = vfs_find_in(fname, parent);
    if (tidx < 0) tidx = vfs_create_in(fname, 0, parent);
    if (tidx < 0) { sh_puts(sh, "write: failed", COL_SHELL_TEXT_ERR); return; }
    uint32_t len = sh_strlen(text);
    vfs_write(tidx, text, len);
    sh_puts(sh, "written", COL_SHELL_TEXT_OK);
}

static void cmd_clear(shell_t *sh) {
    sh->line_head  = 0;
    sh->line_count = 0;
    sh->scroll     = 0;
    sh_memset(sh->lines, 0, sizeof(sh->lines));
}

static void cmd_ver(shell_t *sh) {
    sh_puts(sh, "Velox OS  v0.1", COL_SHELL_TEXT_OK);
    sh_puts(sh, "Shell     v1.0", COL_SHELL_TEXT);
    sh_puts(sh, "Arch      x86-64", COL_SHELL_TEXT);
}

static void shell_exec(shell_t *sh, const char *cmd) {
    cmd = sh_skip(cmd);
    if (!*cmd) return;

    char verb[32];
    const char *arg = sh_word(cmd, verb, 32);

    if      (sh_strcmp(verb, "help")    == 0) cmd_help(sh);
    else if (sh_strcmp(verb, "ls")      == 0) cmd_ls(sh, arg);
    else if (sh_strcmp(verb, "dir")     == 0) cmd_ls(sh, arg);
    else if (sh_strcmp(verb, "pwd")     == 0) cmd_pwd(sh);
    else if (sh_strcmp(verb, "cd")      == 0) cmd_cd(sh, arg);
    else if (sh_strcmp(verb, "cat")     == 0) cmd_cat(sh, arg);
    else if (sh_strcmp(verb, "type")    == 0) cmd_cat(sh, arg);
    else if (sh_strcmp(verb, "echo")    == 0) cmd_echo(sh, arg);
    else if (sh_strcmp(verb, "touch")   == 0) cmd_touch(sh, arg);
    else if (sh_strcmp(verb, "mkdir")   == 0) cmd_mkdir(sh, arg);
    else if (sh_strcmp(verb, "rm")      == 0) cmd_rm(sh, arg);
    else if (sh_strcmp(verb, "del")     == 0) cmd_rm(sh, arg);
    else if (sh_strcmp(verb, "mv")      == 0) cmd_mv(sh, arg);
    else if (sh_strcmp(verb, "ren")     == 0) cmd_mv(sh, arg);
    else if (sh_strcmp(verb, "write")   == 0) cmd_write(sh, arg);
    else if (sh_strcmp(verb, "clear")   == 0) cmd_clear(sh);
    else if (sh_strcmp(verb, "cls")     == 0) cmd_clear(sh);
    else if (sh_strcmp(verb, "ver")     == 0) cmd_ver(sh);
    else {
        char buf[SHELL_LINE_MAX];
        int i = 0;
        const char *p = verb; while (*p && i < 30) buf[i++] = *p++;
        const char *s = ": command not found";
        while (*s && i < SHELL_LINE_MAX - 1) buf[i++] = *s++;
        buf[i] = 0;
        sh_puts(sh, buf, COL_SHELL_TEXT_ERR);
    }
}

static void hist_push(shell_t *sh, const char *cmd) {
    if (!*cmd) return;
    if (sh->hist_count > 0) {
        int last = (sh->hist_count - 1) % SHELL_HIST_MAX;
        if (sh_strcmp(sh->history[last], cmd) == 0) {
            sh->hist_cursor = -1;
            return;
        }
    }
    int idx = sh->hist_count % SHELL_HIST_MAX;
    sh_strcpy(sh->history[idx], cmd, SHELL_HIST_LEN);
    sh->hist_count++;
    sh->hist_cursor = -1;
}

shell_t *shell_create(void) {
    shell_t *sh = (shell_t *)mm_alloc(sizeof(shell_t));
    if (!sh) return 0;
    sh_memset(sh, 0, sizeof(shell_t));
    sh->hist_cursor = -1;
    sh_puts(sh, "Velox Shell v1.0  --  type 'help' for commands", COL_SHELL_TEXT_OK);
    sh_puts(sh, "", COL_SHELL_TEXT);
    return sh;
}

void shell_destroy(shell_t *sh) {
    if (sh) mm_free(sh);
}

void shell_draw(shell_t *sh, window_t *win, int active) {
    if (!win->visible) return;

    int wx = win->x, wy = win->y, ww = win->w, wh = win->h;
    fb_fill_rect(wx, wy + TITLEBAR_HEIGHT, ww, wh - TITLEBAR_HEIGHT, COL_SHELL_BG);
    int out_y    = wy + TITLEBAR_HEIGHT + SHELL_PADDING;
    int out_h    = wh - TITLEBAR_HEIGHT - SHELL_PADDING * 2 - SHELL_INPUT_H - 4;
    int out_x    = wx + SHELL_PADDING;
    int max_cols = (ww - SHELL_PADDING * 2) / 9;
    int max_rows = out_h / SHELL_LINE_H;
    if (max_rows < 1) max_rows = 1;
    if (max_cols < 1) max_cols = 1;
    int total = sh->line_count < SHELL_MAX_LINES ? sh->line_count : SHELL_MAX_LINES;
    int start = total - max_rows - sh->scroll;
    if (start < 0) start = 0;

    for (int r = 0; r < max_rows; r++) {
        int li = start + r;
        if (li >= total) break;
        int ring = (sh->line_head - total + li + SHELL_MAX_LINES) % SHELL_MAX_LINES;
        int py = out_y + r * SHELL_LINE_H;
        text_puts(out_x, py, sh->lines[ring], sh->line_colors[ring], 0, 1);
        (void)max_cols;
    }

    int sep_y = wy + wh - SHELL_INPUT_H - SHELL_PADDING - 2;
    fb_draw_hline(wx + SHELL_PADDING, sep_y, ww - SHELL_PADDING * 2, COL_SHELL_INPUT_BD);

    int in_y  = wy + wh - SHELL_INPUT_H - SHELL_PADDING / 2;
    fb_fill_rect(wx + SHELL_PADDING - 1, in_y - 1,
                 ww - SHELL_PADDING * 2 + 2, SHELL_INPUT_H + 2, COL_SHELL_INPUT_BG);
    fb_draw_rect(wx + SHELL_PADDING - 1, in_y - 1,
                 ww - SHELL_PADDING * 2 + 2, SHELL_INPUT_H + 2, COL_SHELL_INPUT_BD);

    char prompt[48];
    sh_build_prompt(sh, prompt, 48);
    int prompt_px = sh_strlen(prompt) * 9;
    text_puts(wx + SHELL_PADDING + 1, in_y + (SHELL_INPUT_H - 8) / 2,
              prompt, COL_SHELL_PROMPT, 0, 1);

    int in_text_x = wx + SHELL_PADDING + 1 + prompt_px;
    text_puts(in_text_x, in_y + (SHELL_INPUT_H - 8) / 2,
              sh->input, COL_SHELL_TEXT, 0, 1);

    if (active) {
        int cur_x = in_text_x + sh->input_len * 9;
        fb_fill_rect(cur_x, in_y + 2, 2, SHELL_INPUT_H - 4, COL_SHELL_CURSOR);
    }
}

int shell_handle_key(shell_t *sh, window_t *win, const key_event_t *evt) {
    (void)win;
    if (!evt->pressed) return 0;

    switch (evt->scancode) {

    case KEY_ENTER: {
        char echo_buf[SHELL_LINE_MAX];
        char prompt[48];
        sh_build_prompt(sh, prompt, 48);
        int pi = 0, ei = 0;
        const char *pp = prompt;
        while (*pp && ei < SHELL_LINE_MAX - 1) echo_buf[ei++] = *pp++;
        const char *ip = sh->input;
        while (*ip && ei < SHELL_LINE_MAX - 1) echo_buf[ei++] = *ip++;
        echo_buf[ei] = 0;
        sh_puts(sh, echo_buf, COL_SHELL_PROMPT);
        (void)pi;

        hist_push(sh, sh->input);
        shell_exec(sh, sh->input);

        sh->input[0]  = 0;
        sh->input_len = 0;
        desktop.needs_full_redraw = 1;
        return 1;
    }

    case KEY_BACKSPACE:
        if (sh->input_len > 0) {
            sh->input[--sh->input_len] = 0;
            desktop.needs_full_redraw = 1;
        }
        return 1;

    case KEY_PAGE_UP: {
        int max_hist = sh->hist_count < SHELL_HIST_MAX ? sh->hist_count : SHELL_HIST_MAX;
        if (sh->hist_cursor < max_hist - 1) sh->hist_cursor++;
        if (sh->hist_cursor >= 0 && sh->hist_cursor < max_hist) {
            int idx = (sh->hist_count - 1 - sh->hist_cursor) % SHELL_HIST_MAX;
            if (idx < 0) idx += SHELL_HIST_MAX;
            sh_strcpy(sh->input, sh->history[idx], SHELL_INPUT_MAX + 1);
            sh->input_len = sh_strlen(sh->input);
        }
        desktop.needs_full_redraw = 1;
        return 1;
    }

    case KEY_PAGE_DOWN: {
        if (sh->hist_cursor > 0) {
            sh->hist_cursor--;
            int idx = (sh->hist_count - 1 - sh->hist_cursor) % SHELL_HIST_MAX;
            if (idx < 0) idx += SHELL_HIST_MAX;
            sh_strcpy(sh->input, sh->history[idx], SHELL_INPUT_MAX + 1);
            sh->input_len = sh_strlen(sh->input);
        } else {
            sh->hist_cursor = -1;
            sh->input[0]    = 0;
            sh->input_len   = 0;
        }
        desktop.needs_full_redraw = 1;
        return 1;
    }

    case KEY_ESCAPE:
        sh->input[0]  = 0;
        sh->input_len = 0;
        sh->hist_cursor = -1;
        desktop.needs_full_redraw = 1;
        return 1;

    default:
        if (evt->ascii && sh->input_len < SHELL_INPUT_MAX) {
            sh->input[sh->input_len++] = evt->ascii;
            sh->input[sh->input_len]   = 0;
            desktop.needs_full_redraw  = 1;
            return 1;
        }
        return 0;
    }
}