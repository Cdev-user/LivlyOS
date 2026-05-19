
#include "../../kernel/console.h"
#include "../../kernel/console.h"
#include "../../kernel/kernel.h"
#include "../../mm/kmalloc.h"
#include "../../drivers/fs/fat.h"
#include "../../drivers/keyboard.h"
#include "../../kernel/timer.h"
#include "editor.h"
#include "../../kernel/vga.h"


static int editor_status_row(void) { return console_get_height() - 2; }
static int editor_view_height(void) { return console_get_height() - 2; }

typedef struct {
    char    lines[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
    int     line_len[EDITOR_MAX_LINES];
    int     line_count;
    int     cursor_x;
    int     cursor_y;
    int     view_top;
    uint8_t dirty;
    uint8_t line_dirty[EDITOR_MAX_LINES];
    uint8_t status_dirty;
    char    filename[64];
    char undo_lines[UNDO_SIZE][EDITOR_LINE_LEN];
    int  undo_lens[UNDO_SIZE];
    int  undo_cx[UNDO_SIZE];
    int  undo_cy[UNDO_SIZE];
    int  undo_top;
    int  undo_count;
    char redo_lines[UNDO_SIZE][EDITOR_LINE_LEN];
    int  redo_lens[UNDO_SIZE];
    int  redo_cx[UNDO_SIZE];
    int  redo_cy[UNDO_SIZE];
    int  redo_top;
    int  redo_count;
    int  last_cursor_x;
    int  last_cursor_y;
    uint8_t last_dirty;
    int     sel_start_x;
    int     sel_start_y;
    int     sel_active;
    char clipboard[EDITOR_MAX_LINES * EDITOR_LINE_LEN];
    int  clipboard_len;
} EditorState;

static EditorState *ed = 0;
static void ed_save_undo();
static void ed_insert_char(char c);
static void ed_insert_newline();


//─────Delete-Selection─────────────────────────────────────────────
static void ed_delete_selection() {
    if(!ed->sel_active) return;             //Guard 
    int sx = ed->sel_start_x, sy = ed->sel_start_y;
    int ex = ed->cursor_x, ey = ed->cursor_y;
        int temp;
        temp = sx; sx = ex; ex = temp;
        temp = sy; sy = ey; ey = temp;

        ed_save_undo();
        ed->cursor_x = sx;
        ed->cursor_y = sy;

        if(sy == ey) {
            int del = ex - sx;
             for (int i = sx; i < ed->line_len[sy] - del; i++)
            ed->lines[sy][i] = ed->lines[sy][i + del];
        ed->line_len[sy] -= del;
        ed->lines[sy][ed->line_len[sy]] = '\0';
        ed->line_dirty[sy] = 1;
    } else {
        // Multipel rows
        // Bringing the end from the last line to the first 
        int tail_len = ed->line_len[ey] - ex;
        if (sx + tail_len < EDITOR_LINE_LEN - 1) {
            for (int i = 0; i < tail_len; i++)
                ed->lines[sy][sx + i] = ed->lines[ey][ex + i];
            ed->line_len[sy] = sx + tail_len;
            ed->lines[sy][ed->line_len[sy]] = '\0';
        } else {
            ed->line_len[sy] = sx;
            ed->lines[sy][sx] = '\0';
        }
        // Remove rows until sy+1 
        int remove = ey - sy;
        for (int i = sy + 1; i < ed->line_count - remove; i++) {
            for (int j = 0; j <= ed->line_len[i + remove]; j++)
                ed->lines[i][j] = ed->lines[i + remove][j];
            ed->line_len[i] = ed->line_len[i + remove];
        }
        ed->line_count -= remove;
        for (int i = sy; i < ed->line_count + 1; i++)
            ed->line_dirty[i] = 1;
    }

    ed->sel_active = 0;
    ed->dirty      = 1;
    ed->status_dirty = 1;
        
}


//────────Copy──────────────────────
static void ed_copy_selection() {
    if (!ed->sel_active) return;

    int sx = ed->sel_start_x, sy = ed->sel_start_y;
    int ex = ed->cursor_x,    ey = ed->cursor_y;
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp;
        tmp = sx; sx = ex; ex = tmp;
        tmp = sy; sy = ey; ey = tmp;
    }

    int p = 0;
    int max = EDITOR_MAX_LINES * EDITOR_LINE_LEN - 1;
    for (int y = sy; y <= ey && p < max; y++) {
        int start = (y == sy) ? sx : 0;
        int end   = (y == ey) ? ex : ed->line_len[y];
        for (int x = start; x < end && p < max; x++)
            ed->clipboard[p++] = ed->lines[y][x];
        if (y < ey && p < max)
            ed->clipboard[p++] = '\n';
    }
    ed->clipboard[p]  = '\0';
    ed->clipboard_len = p;
}

// ── Paste ───────────────────────────────────────────────
static void ed_paste() {
    if (ed->clipboard_len == 0) return;
    for (int i = 0; i < ed->clipboard_len; i++) {
        if (ed->clipboard[i] == '\n')
            ed_insert_newline();
        else
            ed_insert_char(ed->clipboard[i]);
    }
}



static void ed_clear_selection() {
    if (!ed->sel_active) return;
    int min_y = ed->sel_start_y < ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
    int max_y = ed->sel_start_y > ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
    for (int i = min_y; i <= max_y; i++) ed->line_dirty[i] = 1;
    ed->sel_active = 0;
}

static int ed_is_selected(int x, int y) {
    if (!ed->sel_active) return 0;

    // Normalise
    int sx = ed->sel_start_x, sy = ed->sel_start_y;
    int ex = ed->cursor_x,    ey = ed->cursor_y;

    // If start is end switch it
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp;
        tmp = sx; sx = ex; ex = tmp;
        tmp = sy; sy = ey; ey = tmp;
    }

    if (y < sy || y > ey) return 0;
    if (y == sy && y == ey) return x >= sx && x < ex;
    if (y == sy) return x >= sx;
    if (y == ey) return x < ex;
    return 1;
}


// ── Undo ───────────────────────────────────────────────────
static void ed_save_undo() {
    int y    = ed->cursor_y;
    int slot = ed->undo_top;
    for (int i = 0; i <= ed->line_len[y]; i++)
        ed->undo_lines[slot][i] = ed->lines[y][i];
    ed->undo_lens[slot] = ed->line_len[y];
    ed->undo_cx[slot]   = ed->cursor_x;
    ed->undo_cy[slot]   = ed->cursor_y;
    ed->undo_top        = (ed->undo_top + 1) % UNDO_SIZE;
    if (ed->undo_count < UNDO_SIZE) ed->undo_count++;
    // Neue Aktion → Redo leeren
    ed->redo_top = 0; ed->redo_count = 0;
}

static void ed_do_undo() {
    if (ed->undo_count == 0) return;
    // Aktuellen Stand in Redo sichern
    int y     = ed->cursor_y;
    int rslot = ed->redo_top;
    for (int i = 0; i <= ed->line_len[y]; i++)
    ed->redo_lines[rslot][i] = ed->lines[y][i];
    ed->redo_lens[rslot] = ed->line_len[y];
    ed->redo_cx[rslot]   = ed->cursor_x;
    ed->redo_cy[rslot]   = ed->cursor_y;
    ed->redo_top         = (ed->redo_top + 1) % UNDO_SIZE;
    if (ed->redo_count < UNDO_SIZE) ed->redo_count++;
    // Undo anwenden
    ed->undo_top = (ed->undo_top + UNDO_SIZE - 1) % UNDO_SIZE;
    ed->undo_count--;
    int slot = ed->undo_top;
    int uy   = ed->undo_cy[slot];
    for (int i = 0; i <= ed->undo_lens[slot]; i++)
        ed->lines[uy][i] = ed->undo_lines[slot][i];
    ed->line_len[uy]   = ed->undo_lens[slot];
    ed->cursor_x       = ed->undo_cx[slot];
    ed->cursor_y       = uy;
    ed->line_dirty[uy] = 1;
    ed->status_dirty   = 1;
}

static void ed_do_redo() {
    if (ed->redo_count == 0) return;
    ed->redo_top = (ed->redo_top + UNDO_SIZE - 1) % UNDO_SIZE;
    ed->redo_count--;
    int slot = ed->redo_top;
    int ry   = ed->redo_cy[slot];
    for (int i = 0; i <= ed->redo_lens[slot]; i++)
        ed->lines[ry][i] = ed->redo_lines[slot][i];
    ed->line_len[ry]   = ed->redo_lens[slot];
    ed->cursor_x       = ed->redo_cx[slot];
    ed->cursor_y       = ry;
    ed->line_dirty[ry] = 1;
    ed->status_dirty   = 1;
}

// ── Statusbar ──────────────────────────────────────────────
static void ed_draw_status() {
    // Only redraw if something changed
    int changed = (ed->cursor_x != ed->last_cursor_x ||
                   ed->cursor_y != ed->last_cursor_y  ||
                   ed->dirty    != ed->last_dirty);
    if (!changed) return;

    ed->last_cursor_x = ed->cursor_x;
    ed->last_cursor_y = ed->cursor_y;
    ed->last_dirty    = ed->dirty;


    // Format: "filename* | L:xx C:xx | Ctrl+S=Save Z=Undo Y=Redo"


    // * Update at position 20
    console_write_at(20, editor_status_row(), ed->dirty ? '*' : ' ');

    // L:xx At position 24
    int z = ed->cursor_y + 1;
    console_write_at(24, editor_status_row(), '0' + (z / 100) % 10);
    console_write_at(25, editor_status_row(), '0' + (z / 10)  % 10);
    console_write_at(26, editor_status_row(), '0' + z % 10);

    // C:xx at position 29
    int s2 = ed->cursor_x + 1;
    console_write_at(29, editor_status_row(), '0' + (s2 / 100) % 10);
    console_write_at(30, editor_status_row(), '0' + (s2 / 10)  % 10);
    console_write_at(31, editor_status_row(), '0' + s2 % 10);

    
    
}

// ── Render ─────────────────────────────────────────────────
static void ed_render() {
    for (int sy = 0; sy < editor_view_height(); sy++) {
        int ty = ed->view_top + sy;
        if (ty >= EDITOR_MAX_LINES) continue;

        int needs_redraw = ed->line_dirty[ty];
        int min_sel = ed->sel_start_y < ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
        int max_sel = ed->sel_start_y > ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
        if (ed->sel_active && ty >= min_sel && ty <= max_sel) needs_redraw = 1;

        if (needs_redraw) {
            int len = (ty < ed->line_count) ? ed->line_len[ty] : 0;
            for (int x = 0; x < len; x++) {
                uint8_t color = ed_is_selected(x, ty) ? COLOR_SELECTED : COLOR_NORMAL;
                console_write_at_color(x, sy, ed->lines[ty][x], color);
            }
            for (int x = len; x < console_get_width(); x++)
                console_write_at_color(x, sy, ' ', COLOR_NORMAL);
            ed->line_dirty[ty] = 0;
        }
    }
    if (ed->status_dirty) {
        ed_draw_status();
        ed->status_dirty = 0;
    }
    console_set_pos(ed->cursor_x, ed->cursor_y - ed->view_top);
}


// ──Will get only called 1 time─────────────────────────────────────────────────
static void ed_draw_status_full() {
    char status[80];
    int p = 0;
    // Dateiname (20 Zeichen, mit Leerzeichen aufgefüllt)
    int fn = 0;
    while (ed->filename[fn] && p < 20) status[p++] = ed->filename[fn++];
    while (p < 21) status[p++] = ' ';  // Platz für *
    // Rest fix
    char *rest = " | L:    C:    | Ctrl+S Z=Undo Y=Redo";
    for (int i = 0; rest[i] && p < 79; i++) status[p++] = rest[i];
    status[p] = '\0';
    console_clear_line(editor_status_row());
    console_write_str_at(0, editor_status_row(), status);
    // Danach sofort die Zahlen reinschreiben
    ed->last_cursor_x = -1;  // erzwingt Update
    ed->last_cursor_y = -1;
    ed->last_dirty    = 2;
    ed_draw_status();
}


static void ed_render_full() {
    for (int i = 0; i < EDITOR_MAX_LINES; i++) ed->line_dirty[i] = 1;
    ed->status_dirty = 1;
    for (int x = 0; x < console_get_width(); x++)
        console_write_at(x, editor_view_height(), '-');
    ed_draw_status_full();
    ed_render();
}



// ── Scroll ─────────────────────────────────────────────────
static void ed_scroll_to_cursor() {
    int old_top = ed->view_top;
    if (ed->cursor_y < ed->view_top)
        ed->view_top = ed->cursor_y;
    if (ed->cursor_y >= ed->view_top + editor_view_height())
        ed->view_top = ed->cursor_y - editor_view_height() + 1;
    if (ed->view_top != old_top) ed_render_full();
}

// ── Buffer ↔ Zeilen ────────────────────────────────────────
static void ed_buf_to_lines(uint8_t *buf, uint32_t size) {
    ed->line_count = 0;
    int x = 0;
    for (uint32_t i = 0; i <= size; i++) {
        if (i == size || buf[i] == '\n') {
            ed->lines[ed->line_count][x] = '\0';
            ed->line_len[ed->line_count] = x;
            ed->line_count++;
            x = 0;
            if (ed->line_count >= EDITOR_MAX_LINES) break;
        } else {
            if (x < EDITOR_LINE_LEN - 1)
                ed->lines[ed->line_count][x++] = (char)buf[i];
        }
    }
    if (ed->line_count == 0) { ed->line_count = 1; ed->line_len[0] = 0; }
}

static uint32_t ed_lines_to_buf(uint8_t *buf, uint32_t max) {
    uint32_t offset = 0;
    for (int y = 0; y < ed->line_count && offset < max; y++) {
        for (int x = 0; x < ed->line_len[y] && offset < max; x++)
            buf[offset++] = (uint8_t)ed->lines[y][x];
        if (offset < max) buf[offset++] = '\n';
    }
    return offset;
}

// ── Text-Operationen ───────────────────────────────────────
static void ed_insert_char(char c) {
    ed_clear_selection(); 
    int y = ed->cursor_y, x = ed->cursor_x, len = ed->line_len[y];
    if (len >= EDITOR_LINE_LEN - 1) return;
    ed_save_undo();
    for (int i = len; i > x; i--) ed->lines[y][i] = ed->lines[y][i-1];
    ed->lines[y][x] = c;
    ed->lines[y][len+1] = '\0';
    ed->line_len[y]++;
    ed->cursor_x++;
    ed->dirty = 1;
    ed->line_dirty[y] = 1;
    ed->status_dirty  = 1;
}

static void ed_insert_newline() {
    int y = ed->cursor_y, x = ed->cursor_x, len = ed->line_len[y];
    if (ed->line_count >= EDITOR_MAX_LINES) return;
    ed_save_undo();
    for (int i = ed->line_count; i > y + 1; i--) {
        for (int j = 0; j <= ed->line_len[i-1]; j++)
            ed->lines[i][j] = ed->lines[i-1][j];
        ed->line_len[i] = ed->line_len[i-1];
    }
    int new_len = len - x;
    for (int i = 0; i < new_len; i++) ed->lines[y+1][i] = ed->lines[y][x+i];
    ed->lines[y+1][new_len] = '\0';
    ed->line_len[y+1] = new_len;
    ed->lines[y][x] = '\0';
    ed->line_len[y] = x;
    ed->line_count++;
    ed->cursor_y++;
    ed->cursor_x = 0;
    ed->dirty = 1;
    for (int i = y; i < ed->line_count; i++) ed->line_dirty[i] = 1;
    ed->status_dirty = 1;
}

static void ed_backspace() {
    int y = ed->cursor_y, x = ed->cursor_x;
    ed_save_undo();
    if (x > 0) {
        for (int i = x-1; i < ed->line_len[y]-1; i++)
            ed->lines[y][i] = ed->lines[y][i+1];
        ed->line_len[y]--;
        ed->lines[y][ed->line_len[y]] = '\0';
        ed->cursor_x--;
        ed->line_dirty[y] = 1;
    } else if (y > 0) {
        int prev_len = ed->line_len[y-1];
        int cur_len  = ed->line_len[y];
        if (prev_len + cur_len < EDITOR_LINE_LEN - 1) {
            for (int i = 0; i < cur_len; i++)
                ed->lines[y-1][prev_len+i] = ed->lines[y][i];
            ed->lines[y-1][prev_len+cur_len] = '\0';
            ed->line_len[y-1] = prev_len + cur_len;
            for (int i = y; i < ed->line_count-1; i++) {
                for (int j = 0; j <= ed->line_len[i+1]; j++)
                    ed->lines[i][j] = ed->lines[i+1][j];
                ed->line_len[i] = ed->line_len[i+1];
            }
            ed->line_count--;
            ed->cursor_y--;
            ed->cursor_x = prev_len;
            for (int i = ed->cursor_y; i < ed->line_count+1; i++)
                ed->line_dirty[i] = 1;
        }
    }
    ed->dirty = 1;
    ed->status_dirty = 1;
}

// ── Haupt-Loop ─────────────────────────────────────────────
void fat_edit_file(char *name) {
    if (ed == 0) {
        ed = (EditorState*)kmalloc(sizeof(EditorState));
        if (!ed) { kprintf("Editor: kein Speicher\n"); return; }
    }

    // Reset
    ed->cursor_x = ed->cursor_y = ed->view_top = 0;
    ed->dirty = 0;
    ed->undo_top = 0; ed->undo_count = 0;
    ed->redo_top = 0; ed->redo_count = 0;
    ed->line_count = 0;
    for (int i = 0; i < EDITOR_MAX_LINES; i++) ed->line_dirty[i] = 0;
    ed->status_dirty = 0;
    ed->sel_active = 0;
    ed->sel_start_x = ed->sel_start_y = 0;

    int k = 0;
    while (name[k] && k < 63) { ed->filename[k] = name[k]; k++; }
    ed->filename[k] = '\0';

    uint8_t *buf = kmalloc(EDITOR_MAX_LINES * EDITOR_LINE_LEN);
    if (!buf) { kprintf("kmalloc fehler\n"); return; }
    uint32_t size = 0;
    fat_read_file(ed->filename, buf, &size);
    ed_buf_to_lines(buf, size);
    kfree(buf);

    console_clear();
    ed_render_full();
while (1) {
    uint8_t c = keyboard_read();

    // Shift+Pfeiltasten = Selektion
    if (c == KEY_SHIFT_UP || c == KEY_SHIFT_DOWN ||
        c == KEY_SHIFT_LEFT || c == KEY_SHIFT_RIGHT) {
        if (!ed->sel_active) {
            ed->sel_start_x = ed->cursor_x;
            ed->sel_start_y = ed->cursor_y;
            ed->sel_active  = 1;
        }
        if (c == KEY_SHIFT_UP && ed->cursor_y > 0) {
            ed->cursor_y--;
            if (ed->cursor_x > ed->line_len[ed->cursor_y])
                ed->cursor_x = ed->line_len[ed->cursor_y];
        } else if (c == KEY_SHIFT_DOWN && ed->cursor_y < ed->line_count-1) {
            ed->cursor_y++;
            if (ed->cursor_x > ed->line_len[ed->cursor_y])
                ed->cursor_x = ed->line_len[ed->cursor_y];
        } else if (c == KEY_SHIFT_LEFT) {
            if (ed->cursor_x > 0) ed->cursor_x--;
            else if (ed->cursor_y > 0) { ed->cursor_y--; ed->cursor_x = ed->line_len[ed->cursor_y]; }
        } else if (c == KEY_SHIFT_RIGHT) {
            if (ed->cursor_x < ed->line_len[ed->cursor_y]) ed->cursor_x++;
            else if (ed->cursor_y < ed->line_count-1) { ed->cursor_y++; ed->cursor_x = 0; }
        }
        int min_y = ed->sel_start_y < ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
        int max_y = ed->sel_start_y > ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
        for (int i = min_y; i <= max_y; i++) ed->line_dirty[i] = 1;
        ed->status_dirty = 1;

    // Normal Arrow keys = Selection mode off
    } else if (c == KEY_UP || c == KEY_DOWN || c == KEY_LEFT || c == KEY_RIGHT) {
        if (ed->sel_active) {
            int min_y = ed->sel_start_y < ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
            int max_y = ed->sel_start_y > ed->cursor_y ? ed->sel_start_y : ed->cursor_y;
            for (int i = min_y; i <= max_y; i++) ed->line_dirty[i] = 1;
            ed->sel_active = 0;
        }
        if      (c == KEY_UP)    { if (ed->cursor_y > 0) { ed->cursor_y--; if (ed->cursor_x > ed->line_len[ed->cursor_y]) ed->cursor_x = ed->line_len[ed->cursor_y]; ed->status_dirty = 1; } }
        else if (c == KEY_DOWN)  { if (ed->cursor_y < ed->line_count-1) { ed->cursor_y++; if (ed->cursor_x > ed->line_len[ed->cursor_y]) ed->cursor_x = ed->line_len[ed->cursor_y]; ed->status_dirty = 1; } }
        else if (c == KEY_LEFT)  { if (ed->cursor_x > 0) { ed->cursor_x--; ed->status_dirty = 1; } else if (ed->cursor_y > 0) { ed->cursor_y--; ed->cursor_x = ed->line_len[ed->cursor_y]; ed->status_dirty = 1; } }
        else if (c == KEY_RIGHT) { if (ed->cursor_x < ed->line_len[ed->cursor_y]) { ed->cursor_x++; ed->status_dirty = 1; } else if (ed->cursor_y < ed->line_count-1) { ed->cursor_y++; ed->cursor_x = 0; ed->status_dirty = 1; } }

    } else if (c == KEY_CTRL_S) {
        uint32_t sz = ed->line_count * EDITOR_LINE_LEN + 16;
        uint8_t *sb = kmalloc(sz);
        if (sb) {
        uint64_t t1 = get_ms();
        uint32_t w = ed_lines_to_buf(sb, sz);
        uint64_t t2 = get_ms();
        fat_write_data(ed->filename, sb, w);
        uint64_t t3 = get_ms();
        kfree(sb);
        ed->dirty = 0;
        ed->status_dirty = 1;

        // t2-t1 = ed_lines_to_buf, t3-t2 = fat_write_data
        uint32_t ms1 = (uint32_t)(t2 - t1);
        uint32_t ms2 = (uint32_t)(t3 - t2);
        // Zeile 1: buf zeit
        console_write_at(60, 1, '0' + (ms1 / 100) % 10);
        console_write_at(61, 1, '0' + (ms1 / 10)  % 10);
        console_write_at(62, 1, '0' + ms1 % 10);
        console_write_at(63, 1, 'b');
        // Zeile 1: write zeit
        console_write_at(65, 1, '0' + (ms2 / 100) % 10);
        console_write_at(66, 1, '0' + (ms2 / 10)  % 10);
        console_write_at(67, 1, '0' + ms2 % 10);
        console_write_at(68, 1, 'w');
    }
    } else if (c == KEY_CTRL_C) {
        ed_copy_selection();

    } else if (c == KEY_CTRL_V) {
        ed_paste();

    } else if (c == KEY_CTRL_Z) {
        ed_do_undo();

    } else if (c == KEY_CTRL_Y) {
        ed_do_redo();

    } else if (c == '\n') {
        ed_delete_selection();
        ed_insert_newline();

    } else if (c == '\b') {
        if (ed->sel_active) ed_delete_selection();
        else                ed_backspace();

    } else if (c == 0x1B) {
        console_clear(); break;

    } else if (c >= 0x20 && c < 0x80) {
        ed_delete_selection();
        ed_insert_char(c);
    }

    ed_scroll_to_cursor();
    ed_render();
}
}
