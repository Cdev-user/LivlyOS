#include "fb_console.h"
#include "framebuffer.h"
#include "../../lib/stlib/types.h"

static fb_console_t g_console;

#define fg_standart COLOR_WHITE
#define bg_standart COLOR_BLACK                  
//standart farben für weniger hex scheiße


static int last_cursor_col = -1;
static int last_cursor_row = -1;

bool framebuffer_check(framebuffer_t *fb) {
    return fb &&
           fb->address &&
           fb->bpp > 0 &&
           fb->height > 0 &&
           fb->pitch > 0 &&
           fb->width > 0;
}



/* Klassische VGA Palette: Index 0-15 → RGB */
static const uint32_t vga_palette[16] = {
    0x000000,  /* 0  schwarz       */
    0x0000AA,  /* 1  blau          */
    0x00AA00,  /* 2  grün          */
    0x00AAAA,  /* 3  cyan          */
    0xAA0000,  /* 4  rot           */
    0xAA00AA,  /* 5  magenta       */
    0xAA5500,  /* 6  braun         */
    0xAAAAAA,  /* 7  hellgrau      */
    0x555555,  /* 8  dunkelgrau    */
    0x5555FF,  /* 9  hellblau      */
    0x55FF55,  /* 10 hellgrün      */
    0x55FFFF,  /* 11 hellcyan      */
    0xFF5555,  /* 12 hellrot       */
    0xFF55FF,  /* 13 hellmagenta   */
    0xFFFF55,  /* 14 gelb          */
    0xFFFFFF,  /* 15 weiß          */
};




void fb_console_init(framebuffer_t *fb) {
    // TODO: g_console füllen
    // - fb speichern
    // - cursor auf 0,0
    // - max_cols = fb->width / 8
    // - max_rows = fb->height / 16
    // - fg = COLOR_WHITE, bg = COLOR_BLACK
    // - fb_clear(fb, COLOR_BLACK) aufrufen

    
    g_console.fb = fb;              // Pointer auf den Framebuffer speichern

    if(framebuffer_check(fb)) {
    g_console.cursor_col = 0;        
    g_console.cursor_row = 0;        
    g_console.max_cols  = fb->width / 8;           //Unterteilt wie viele zeichen nebeneinander passen in einer reihe (ein ASCII ist 8 pixel breit)
    g_console.max_rows  = fb->height / 16;         //jeder ASCII ist 16pixel hoch deswegen durch 16 damit man alle zeilen berechnen kann
    g_console.fg_color  = fg_standart;
    g_console.bg_color  = bg_standart;
    fb_clear(fb,bg_standart);
    } else {
        //irgendein system tick das denn error auslöst oder so 
        //Note braucht ein event handler oder ein err handler oder so
    }
    

}

void fb_console_set_color(uint32_t fg, uint32_t bg) {
    // TODO: in g_console speichern
    fg = fg ? fg : fg_standart;
    bg = bg ? bg : bg_standart;
    g_console.bg_color = bg;
    g_console.fg_color = fg;
}

void fb_console_clear(void) {
    // TODO: fb_clear + cursor auf 0,0
    fb_clear(g_console.fb,g_console.bg_color);
    g_console.cursor_col = 0;
    g_console.cursor_row = 0;
}



static void scroll(void) {
    // Erstmal Stub - macht nichts
    // Implementieren wir später
    g_console.cursor_row = g_console.max_rows - 1;  
}


static void newline(void) {
    // TODO:
    // 1) cursor_col = 0
    // 2) cursor_row++
    // 3) Wenn cursor_row >= max_rows → scroll() aufrufen
    //    (scroll machen wir später, erstmal Stub)

    g_console.cursor_col = 0;
    g_console.cursor_row++;
    if (g_console.cursor_row >= g_console.max_rows) {
        scroll();
    }

}


void fb_console_putchar(char c) {
    // 1) Wenn c == '\n' → newline() aufrufen, return
    // 2) Pixel-Position berechnen aus cursor_col/row:
    //    px_x = cursor_col * 8
    //    px_y = cursor_row * 16
    // 3) fb_draw_char(g_console.fb, px_x, px_y, c, fg_color, bg_color)
    // 4) cursor_col++
    // 5) Wenn cursor_col >= max_cols → newline() aufrufen

    

    if ('\n' == c) {
        newline();
        return;
    }
    /*
    Fehlt:
    '\b' Backspace
    ' '  Space
    '\t' Tab
    '\0' string ende
    '\r' cursor am zeilenanfang plazieren
    */



    int px_x = g_console.cursor_col * 8;
    int px_y = g_console.cursor_row * 16;
    // mal 16 weil wir pro spalte 16 pixel haben 
    // mal 8 weil wir pro zeichen 8 pixel nehmen

    fb_draw_char(g_console.fb, px_x, px_y, c, g_console.fg_color, g_console.bg_color);
    g_console.cursor_col++;

    if(g_console.cursor_col >= g_console.max_cols) newline();

}

void fb_console_print(const char *s) {
    // TODO: für jedes Zeichen putchar aufrufen
    while (*s) {
        fb_console_putchar(*s);
        s++;
    }
}


void fb_console_set_pos(int col, int row) {
    /* 
     * - Schreib col und row in g_console.cursor_col und g_console.cursor_row
     * - Clamp: col darf nicht negativ und nicht >= max_cols sein
     * - Gleiche Logik für row
     */

    if(!(col < 0 || col >= g_console.max_cols)) g_console.cursor_col = col;

    if(!(0 > row || row >= g_console.max_rows)) g_console.cursor_row = row;


}

int fb_console_get_col(void) {
    /* return g_console.cursor_col */
    return g_console.cursor_col;
}

int fb_console_get_row(void) {
    /* return g_console.cursor_row */
    return g_console.cursor_row;
}

void fb_console_backspace(void) {
    framebuffer_t *fb = g_console.fb;
    if (g_console.cursor_col > 0) {
        g_console.cursor_col--;
    } else if (g_console.cursor_row > 0) {
        g_console.cursor_row--;
        g_console.cursor_col = g_console.max_cols - 1;
    } else {
        return;         //nothing to delet
    }
    fb_draw_char(fb, 
             g_console.cursor_col * 8, 
             g_console.cursor_row * 16, 
             ' ', 
             g_console.fg_color, 
             g_console.bg_color);

    
}




void fb_console_set_cursor(int col, int row) {
    framebuffer_t *fb = g_console.fb;
    
    /* Alten Cursor wegmachen */
    if (last_cursor_col >= 0 && last_cursor_row >= 0) {
        int px = last_cursor_col * 8;
        int py = last_cursor_row * 16 + 14;       /* 14-15 = letzten 2 Pixel */
        fb_draw_rect(fb, px, py, 8, 2, g_console.bg_color);
    }
    
    /* Neuen Cursor zeichnen */
    int px = col * 8;
    int py = row * 16 + 14;
    fb_draw_rect(fb, px, py, 8, 2, g_console.fg_color);
    
    /* Merken für nächstes Mal */
    last_cursor_col = col;
    last_cursor_row = row;
}

void fb_console_write_at(int x, int y, char c) {

    int px = x * 8;
    int py = y * 16;
    fb_draw_char(g_console.fb, px, py, c, g_console.fg_color, g_console.bg_color);

}

void fb_console_write_str_at(int x, int y, char *s) {
    while(*s) {
        fb_console_write_at(x++, y, *s++);
    }
}

void fb_console_clear_line(int y) {
    fb_draw_rect(g_console.fb, 0, y * 16, g_console.fb->width, g_console.fb->height, g_console.bg_color);
}

void fb_console_write_at_color(int x, int y, char c, uint8_t color) {
    uint8_t fg_idx = color & 0x0F;          /* untere 4 Bits */
    uint8_t bg_idx = (color >> 4) & 0x0F;   /* obere 4 Bits */
    
    uint32_t fg_rgb = vga_palette[fg_idx];
    uint32_t bg_rgb = vga_palette[bg_idx];
    
    fb_draw_char(g_console.fb, x * 8, y * 16, c, fg_rgb, bg_rgb);
}

int fb_console_get_width(void)  { return g_console.max_cols; }
int fb_console_get_height(void) { return g_console.max_rows; }