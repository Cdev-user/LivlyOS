#include "vga.h"
#include "kernel.h"
#include "io.h"
// Hier kommt der Zustand — welche zwei Variablen brauchst du?
static int row = 0;         //Aktuelle reihe
static int col = 0;         //aktuelle spalte
static uint8_t farbe = 0x07;
int line_length[VGA_HOEHE] = {0};
int cursor_pos; // von 0 bis VGA_BREITE * VGA_HOEHE
void vga_putchar(char c) {
    // Schritt 1: Index berechnen
    // Schritt 2: Zeichen in den Puffer schreiben
    // Schritt 3: Spalte eins weiter


    if(c == '\n'){
        line_length[row] = col;
        col = 0;
        row = row + 1;
    }
    else{
    
        int index = row * VGA_BREITE + col;
        VGA_PUFFER[index] = c | (farbe << 8);
        col++;
            if(col >= VGA_BREITE){
               col = 0;
               row = row + 1;
            }
        }
    if(row >= VGA_HOEHE){

    // Zeilen 1-24 nach oben kopieren (Zeile 0 wird überschrieben)
    for(int z = 0; z < VGA_HOEHE - 1; z++){
        for(int s = 0; s < VGA_BREITE; s++){
            VGA_PUFFER[z * VGA_BREITE + s] = VGA_PUFFER[(z+1) * VGA_BREITE + s];
        }
        line_length[z] = line_length[z + 1];
    }

    // letzte Zeile leeren
    for(int s = 0; s < VGA_BREITE; s++){
        VGA_PUFFER[(VGA_HOEHE-1) * VGA_BREITE + s] = ' ' | (farbe << 8);
    }
    row = VGA_HOEHE - 1;  
}
    
    vga_set_cursor(col, row);
}

void vga_set_cursor(int col, int row) {
    uint16_t pos = row * VGA_BREITE + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));
}

void vga_backspace() {
    if (col > 0) {
        col--;
        

    } else if (row > 0) {  // ← am Zeilenanfang
        row--;
        col = line_length[row];
    } else {
        return;
    }
    VGA_PUFFER[row * VGA_BREITE + col] = ' ' | (farbe << 8);

    line_length[row] = col; 
    
    vga_set_cursor(col,row);
}

void vga_write_at(int x, int y, char c) {
    if(x < 0 || x >= VGA_BREITE) return;
    if(y < 0 || y >= VGA_HOEHE) return;
    VGA_PUFFER[y * VGA_BREITE + x] = (uint16_t)c | (farbe << 8);
}

void vga_clear_line(int y) {
    for (int x = 0; x < VGA_BREITE; x++)
        vga_write_at(x, y, ' ');
}
void vga_set_pos(int x, int y) {
    col = x;                       
    row = y;
    vga_set_cursor(col,row); 
}
void vga_write_str_at(int x, int y, char *s) {
    while(*s && x < VGA_BREITE) {
        vga_write_at(x++,y,*s++);
    }
    
}

void vga_write_at_color(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= VGA_BREITE) return;
    if (y < 0 || y >= VGA_HOEHE)  return;
    VGA_PUFFER[y * VGA_BREITE + x] = (uint16_t)c | ((uint16_t)color << 8);
}

int vga_get_col() { return col; }
int vga_get_row() { return row; }





void vga_setfarbe(uint8_t f){
    farbe = f;
}


void vga_print(char *s){
    while (*s) {
    vga_putchar(*s++);
    }
}

void vga_clear() {
    for (int i = 0; i < VGA_BREITE * VGA_HOEHE; i++) {
        VGA_PUFFER[i] = ' ' | (farbe << 8);
    }
    row = 0;
    col = 0;
}



// ans[i] = str[i] - '0';