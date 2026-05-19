#ifndef FAT_H
#define FAT_H


#include "../../kernel/kernel.h"


typedef struct {
    uint8_t  jump[3];          // Jump-Befehl
    uint8_t  oem[8];           // OEM Name
    uint16_t bytes_per_sector; // meist 512
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors; // wo FAT anfängt
    uint8_t  fat_count;        // meist 2
    uint16_t root_entries;     // 0 für FAT32
    uint16_t total_sectors_16; // 0 für FAT32
    uint8_t  media_type;
    uint16_t fat_size_16;      // 0 für FAT32
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;      // FAT Größe in Sektoren
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;     // erster Cluster des Root-Verzeichnisses
    // ...
} __attribute__((packed)) FAT32BootSector;

typedef struct {
    uint8_t  name[8];       // Dateiname (8 Zeichen, mit Leerzeichen aufgefüllt)
    uint8_t  ext[3];        // Erweiterung (3 Zeichen)
    uint8_t  attributes;    // 0x20 = Datei, 0x10 = Verzeichnis
    uint8_t  reserved[8];   // offset 12-19
    uint16_t cluster_high;  // obere 16 Bits — kommt VOR cluster_low!
    uint16_t time;          // offset 22
    uint16_t date;          // offset 24
    uint16_t cluster_low;   // untere 16 Bits des ersten Clusters
    uint32_t size;          // Dateigröße in Bytes
} __attribute__((packed)) FAT32DirEntry;
// Gesamt: 8+3+1+8+2+2+2+2+4 = 32 Bytes 

void fat_init();
void fat_list_dir();  // für ls Befehl
void fat_write_file(char name[16],char ext[3]);
void fat_create_dir(char name[16]);
void fat_init_dir_cluster(uint32_t cluster, uint32_t parent_cluster);
void str_end(char *name);
void fat_open_dir(char name[8]);
void fat_cd_root();
void fat_cd_back();
void fat_delete_file(char *arg2); 
void fat_rename_file(char *arg,char *arg2); 
void fat_read_file(char *name, uint8_t *buf, uint32_t *size);
//                              ↑ Inhalt     ↑ wie viele Bytes gelesen
void fat_write_data(char *name, uint8_t *buf, uint32_t size);

uint32_t fat_alloc_cluster(uint32_t prev_cluster);
uint32_t fat_next_cluster(uint32_t cluster);
uint32_t fat_extend_chain(uint32_t prev_cluster);

int str_equal_fat(char *fat_name, char *name);


char* fat_get_cwdpath();
#endif