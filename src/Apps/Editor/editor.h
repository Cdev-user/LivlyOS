#ifndef EDITOR_H
#define EDITOR_H

void fat_edit_file(char *name);

#define EDITOR_MAX_LINES   200
#define EDITOR_LINE_LEN     78
#define UNDO_SIZE 50   
#define COLOR_NORMAL VGA_FARBE(VGA_WEISS, VGA_SCHWARZ)
#define COLOR_SELECTED VGA_FARBE(VGA_WEISS,VGA_BLAU)

#endif