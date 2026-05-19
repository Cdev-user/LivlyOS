#include "fat.h"
#include "ata.h"
#include "../../mm/kmalloc.h"
#include "../keyboard.h"
#include "../../Apps/Editor/editor.h"
#include "../../kernel/console.h"

extern int str_equal(char *a, char *b);


// Diese drei Variablen sind "global" für diese Datei (static = nur hier sichtbar)
static FAT32BootSector boot;        // Kopie des Boot-Sektors mit allen FAT-Infos
static uint32_t fat_start;          // Erster Sektor der FAT-Tabelle
static uint32_t data_start;         // Erster Sektor des Datenbereichs (wo Cluster 2 liegt)
static uint32_t root_cluster;       // Cluster-Nummer des Root-Verzeichnisses (meist 2)

static uint32_t cwd_cluster;      // welcher Cluster ist gerade offen
static char cwd_path[256];        // z.B. "/docs/"


static uint32_t cached_fat_sector_nr = 0xFFFFFFFF;  //which sector is cached 
static uint16_t cached_fat_buf[256];                // chached sektor
static uint8_t  fat_cache_dirty = 0;                // did the sektor change

// ============================================================
// fat_init
// Liest den Boot-Sektor und berechnet alle wichtigen Offsets.
// Muss einmal beim Start aufgerufen werden bevor alles andere geht.
// ============================================================
void fat_init() {
    uint16_t buf[256];                          // 256 × 2 Bytes = 512 Bytes = 1 Sektor

    ata_read_sector(FAT32_PARTITION_START, buf); // Boot-Sektor der Partition lesen
                                                 // FAT32_PARTITION_START = 2048 (in ata.h definiert)

    boot = *(FAT32BootSector*)buf;              // buf als FAT32BootSector-Struct interpretieren
                                                // und in unsere globale Variable kopieren

    // FAT fängt direkt nach den reservierten Sektoren an
    // reserved_sectors ist im Boot-Sektor gespeichert (meist 32)
    fat_start = FAT32_PARTITION_START + boot.reserved_sectors;

    // Datenbereich fängt nach allen FAT-Kopien an
    // fat_count = Anzahl FAT-Kopien (meist 2), fat_size_32 = Sektoren pro FAT
    data_start = fat_start + (boot.fat_count * boot.fat_size_32);

    // Root-Verzeichnis liegt in diesem Cluster (meist Cluster 2)
    root_cluster = boot.root_cluster;
    cwd_cluster = root_cluster;
    cwd_path[0] = '/';
    cwd_path[1] = '\0';
}



static void fat_cache_flush() {
    if (fat_cache_dirty) {
        ata_write_sector(cached_fat_sector_nr, cached_fat_buf);
        fat_cache_dirty = 0;
    }
}

// ============================================================
// fat_list_dir
// Gibt alle Dateien im Root-Verzeichnis aus (der ls-Befehl)
// ============================================================
void fat_list_dir() {
    uint16_t buf[256];                          // Puffer für einen Sektor
    uint32_t current_cluster = cwd_cluster;    // Fange beim Root-Cluster an

    int header_printed = 0;  
    // Solange wir noch einen gültigen Cluster haben
    // >= 0x0FFFFFF8 bedeutet End-of-Chain → kein weiterer Cluster
    while (current_cluster < 0x0FFFFFF8) {

        // Berechne den ersten Sektor dieses Clusters
        // -2 weil Cluster-Nummern bei 2 starten, Sektoren aber bei 0
        uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;

        int done = 0;   // Wird 1 wenn wir einen 0x00-Eintrag finden (Ende der Einträge)

        // Jeden Sektor in diesem Cluster durchgehen
        for (int s = 0; s < boot.sectors_per_cluster && !done; s++) {
            ata_read_sector(sektor + s, buf);               // Sektor in buf laden
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf; // buf als Array von 32-Byte-Einträgen lesen

            // Jeder Sektor hat 512/32 = 16 Directory-Einträge
            for (int i = 0; i < 16; i++) {

                if (eintraege[i].name[0] == 0x00) { done = 1; break;}  // 0x00 = ab hier keine Einträge mehr
                if (eintraege[i].name[0] == 0xE5) continue;            // 0xE5 = gelöschte Datei, überspringen
                if (eintraege[i].name[0] < 0x20)  continue;            // Steuerzeichen = kein normaler Eintrag
                if (eintraege[i].name[0] == '.')  continue;
                if(!header_printed) {
                header_printed = 1;
                }
                // Name ausgeben (8 Zeichen, mit Leerzeichen aufgefüllt)
                if(eintraege[i].attributes == 0x10) {
                    kprintf("[DIR] ");
                    for (int j = 0; j < 8; j++) {
                        if (eintraege[i].name[j] == ' ') break;             // Leerzeichen = Ende des Namens
                        kprintf("%c", eintraege[i].name[j]);
                    }
                    kprintf("\n");
                } else {
                for (int j = 0; j < 8; j++) {
                if (eintraege[i].name[j] == ' ') break;             // Leerzeichen = Ende des Namens
                kprintf("%c", eintraege[i].name[j]);
                }
                kprintf(".");
                // Extension ausgeben (3 Zeichen)
                for (int j = 0; j < 3; j++) {
                    if (eintraege[i].ext[j] == ' ') break;              // Leerzeichen = Ende der Extension
                    kprintf("%c", eintraege[i].ext[j]);
                }
                uint32_t size = eintraege[i].size;

                if(size < 1024) {
                    kprintf("    %u B", size);
                } else if(size < 1024 * 1024) {
                    kprintf("    %u KB", size / 1024);
                } else if(size < 1024 * 1024 * 1024) {
                    kprintf("    %u MB", size / (1024 * 1024));
                } else {
                    kprintf("    %u GB", size / (1024 * 1024 * 1024));
                }
                kprintf("\n");
                }
            }
        }

        if (done) break;    // 0x00 gefunden: kein nächster Cluster nötig

        // Nächsten Cluster aus der FAT-Tabelle lesen und weitermachen
        current_cluster = fat_next_cluster(current_cluster);
    }
}


// ============================================================
// fat_write_file
// Erstellt eine neue Datei im Root-Verzeichnis.
// Sucht einen freien Directory-Eintrag und trägt Name/Ext ein.
// ============================================================
void fat_write_file(char name[16], char ext[3]) {
    static uint16_t buf[256];
    uint32_t current_cluster = cwd_cluster;    // Suche startet im Root-Verzeichnis

    // kprintf("Vor duplikat check\n");
    // Duplikat namens check
    uint32_t chk = cwd_cluster;
    while(chk < 0x0FFFFFF8) {
        uint32_t s = data_start + (chk - 2) * boot.sectors_per_cluster;
        ata_read_sector(s, buf);
        FAT32DirEntry *e = (FAT32DirEntry*)buf;
        for(int i = 0; i < 16; i++) {
            if(e[i].name[0] == 0x00) break;       // ← leer, abbrechen
            if(e[i].name[0] == 0xE5) continue;    // ← gelöscht, überspringen
            if(str_equal_fat((char*)e[i].name, name)) {
                kprintf("Datei existiert bereits\n");
                return;
            }
        }
        chk = fat_next_cluster(chk);
    }


    // kprintf("Nach cuplikat check\n");
    while(1) {
        
        // Ersten Sektor des aktuellen Clusters berechnen
        uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
        
        int foundEntry = 0;     // Wird 1 sobald wir einen freien Slot gefunden haben

        // Alle Sektoren dieses Clusters durchsuchen
        for(int s = 0; s < boot.sectors_per_cluster && !foundEntry; s++) {
             
            ata_read_sector(sektor + s, buf);
            
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;

            for(int i = 0; i < 16; i++) {
                
                // 0x00 = niemals benutzt, 0xE5 = gelöscht → beides ist ein freier Slot
                if(eintraege[i].name[0] == 0x00 || eintraege[i].name[0] == 0xE5) {
                    
                    FAT32DirEntry *slot = &eintraege[i];    // Zeiger auf diesen Eintrag

                    // Name ins 8.3-Format schreiben: erst alles mit Spaces füllen,
                    // dann die echten Zeichen reinschreiben (bis '\0')
                    for (int j = 0; j < 8; j++) slot->name[j] = ' ';
                    for (int j = 0; j < 8 && name[j] != '\0'; j++) slot->name[j] = name[j];
                    for (int j = 0; j < 3; j++) slot->ext[j]  = ' ';
                    for (int j = 0; j < 3 && ext[j]  != '\0'; j++) slot->ext[j]  = ext[j];

                    slot->attributes   = 0x20;              // 0x20 = normale Datei (kein Ordner)
                    slot->size         = 0;                 // Größe 0, weil noch nichts reingeschrieben

                    // Einen freien Cluster für den Dateiinhalt reservieren
                    
                    uint32_t cluster   = fat_alloc_cluster(1);
                    // kprintf("alloc cluster: %u\n", cluster); 
                    
                    // Cluster-Nummer ist 32 Bit, wird in zwei 16-Bit-Felder aufgeteilt
                    slot->cluster_high = cluster >> 16;     // obere 16 Bits
                    slot->cluster_low  = cluster & 0xFFFF;  // untere 16 Bits
                    
                    ata_write_sector(sektor + s, (uint16_t*)buf); // geänderten Sektor zurückschreiben
                    
                    foundEntry = 1;
                    break;
                }
                if(foundEntry) break;
            }
        }

        if(foundEntry) break;   // Fertig, Datei wurde angelegt

        // Kein freier Slot in diesem Cluster → nächsten Cluster der Chain prüfen
        uint32_t next = fat_next_cluster(current_cluster);

        if (next >= 0x0FFFFFF8) {
            // Kein nächster Cluster vorhanden → neuen Cluster anlegen und verlinken
            next = fat_extend_chain(current_cluster);
            if (next >= 0x0FFFFFF8) {
                kprintf("Kein Platz im Root-Directory\n");
                break;
            }
        }

        current_cluster = next; // Weiter mit dem nächsten Cluster
    }
}







// ============================================================
// fat_create_dir
// erstellt einen ordner
// 
// ============================================================
void fat_create_dir(char name[8]) {
    
    for(int j = 0; j < 8 && name[j] != '\0'; j++) kprintf("%c", name[j]);
    kprintf("\n");
    
    static uint16_t buf[256];
    uint32_t current_cluster = cwd_cluster; 

    //Duplikate check
    uint32_t chk = cwd_cluster;
    int chk_done = 0;
while(chk < 0x0FFFFFF8 && !chk_done) {
    uint32_t chk_sektor = data_start + (chk - 2) * boot.sectors_per_cluster;
    for(int s = 0; s < boot.sectors_per_cluster && !chk_done; s++) {
        ata_read_sector(chk_sektor + s, buf);
        FAT32DirEntry *e = (FAT32DirEntry*)buf;
        for(int i = 0; i < 16; i++) {
            if(e[i].name[0] == 0x00) { chk_done = 1; break; }
            if(e[i].name[0] == 0xE5) continue;
            if(str_equal_fat((char*)e[i].name, name)) {
                kprintf("Datei existiert bereits\n");
                return;
            }
        }
    }
    if(!chk_done) chk = fat_next_cluster(chk);
}

    
    int foundEntry = 0;

    while(1) {
    

        uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;

        for(int s = 0; s < boot.sectors_per_cluster && !foundEntry;s++) {

            ata_read_sector(sektor + s, buf);
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;

            for(int i = 0; i < 16; i++) {

                if(eintraege[i].name[0] == 0x00 || eintraege[i].name[0] == 0xE5) {
                FAT32DirEntry *slot = &eintraege[i];
                


                for (int j = 0; j < 8; j++) slot->name[j] = ' ';
                for (int j = 0; j < 8 && name[j] != '\0'; j++) slot->name[j] = name[j];
                for (int j = 0; j < 3; j++) slot->ext[j]  = ' ';
                slot->attributes = 0x10;
                slot->size = 0;

                
                uint32_t cluster = fat_alloc_cluster(1); 
                
                slot->cluster_high = cluster >> 16;
                slot->cluster_low = cluster & 0xFFFF;
                
                ata_write_sector(sektor + s, (uint16_t*)buf); 
                

                fat_init_dir_cluster(cluster, current_cluster);
                
                foundEntry = 1;
                break;
                }
            }
        }
        if(foundEntry) break;

        uint32_t next = fat_next_cluster(current_cluster);
        if(next >= 0x0FFFFFF8) {
            next = fat_extend_chain(current_cluster);
            if(next >= 0x0FFFFFF8) {
                kprintf("Kein Platz mehr Übrig\n");
                break;
            }
        }
        current_cluster = next;


    }
}



// ============================================================
// fat_alloc_cluster
// Reserviert so viele freie Cluster wie für `size` Bytes nötig.
// Verkettet sie als Chain in der FAT und gibt den ersten zurück.
// ============================================================


uint32_t fat_alloc_cluster(uint32_t size) {
    uint32_t cluster_size = boot.sectors_per_cluster * 512;
    uint32_t count = (size + cluster_size - 1) / cluster_size;
    uint32_t data_sector_count = boot.total_sectors_32 - (data_start - FAT32_PARTITION_START);
    uint32_t total_clusters = data_sector_count / boot.sectors_per_cluster;

    uint32_t prev = 0;
    uint32_t first = 0;

    for(uint32_t i = 2; i < total_clusters + 2 && count > 0; i++) {
        uint32_t fat_sector  = fat_start + (i * 4) / 512;
        uint32_t entry_index = (i * 4 % 512) / 4;

        // Cache benutzen
        if(fat_sector != cached_fat_sector_nr) {
            fat_cache_flush();
            cached_fat_sector_nr = fat_sector;
            ata_read_sector(fat_sector, cached_fat_buf);
        }
        uint32_t *fat = (uint32_t*)cached_fat_buf;

        if(fat[entry_index] == 0x00000000) {
            if(first == 0) first = i;

            fat[entry_index] = 0x0FFFFFFF;
            fat_cache_dirty = 1;

            if(prev != 0) {
                uint32_t prev_sector = fat_start + (prev * 4) / 512;
                uint32_t prev_index  = (prev * 4 % 512) / 4;

                if(prev_sector != cached_fat_sector_nr) {
                    fat_cache_flush();
                    cached_fat_sector_nr = prev_sector;
                    ata_read_sector(prev_sector, cached_fat_buf);
                }
                uint32_t *prev_fat = (uint32_t*)cached_fat_buf;
                prev_fat[prev_index] = i;
                fat_cache_dirty = 1;
            }

            prev = i;
            count--;
        }
    }

    fat_cache_flush();  // am Ende alles auf Disk schreiben
    return first;
}

// ============================================================
// fat_next_cluster
// Liest aus der FAT welcher Cluster nach `cluster` kommt.
// Rückgabe >= 0x0FFFFFF8 = Ende der Chain.
// ============================================================
uint32_t fat_next_cluster(uint32_t cluster) {
    uint32_t fat_byte_offset = cluster * 4;
    uint32_t fat_sector      = fat_start + (fat_byte_offset / 512);
    uint32_t entry_index     = (fat_byte_offset % 512) / 4;

    if (fat_sector != cached_fat_sector_nr) {
        fat_cache_flush();              // ← das fehlte noch
        cached_fat_sector_nr = fat_sector;
        ata_read_sector(fat_sector, cached_fat_buf);
    }

    uint32_t *fat = (uint32_t*)cached_fat_buf;
    return fat[entry_index] & 0x0FFFFFFF;
}


// ============================================================
// fat_extend_chain
// Hängt einen neuen leeren Cluster ans Ende der Chain von prev_cluster.
// Wird benutzt wenn ein Directory-Cluster voll ist.
// ============================================================
uint32_t fat_extend_chain(uint32_t prev_cluster) {
    uint32_t data_sector_count = boot.total_sectors_32 - (data_start - FAT32_PARTITION_START);
    uint32_t total_clusters = data_sector_count / boot.sectors_per_cluster;
    uint32_t new_cluster = 0;

    for(uint32_t i = 2; i < total_clusters + 2; i++) {
        uint32_t fat_sektor  = fat_start + (i * 4) / 512;
        uint32_t entry_index = (i * 4 % 512) / 4;

        // Cache benutzen
        if(fat_sektor != cached_fat_sector_nr) {
            fat_cache_flush();
            cached_fat_sector_nr = fat_sektor;
            ata_read_sector(fat_sektor, cached_fat_buf);
        }
        uint32_t *fat = (uint32_t*)cached_fat_buf;

        if(fat[entry_index] == 0x00000000) {
            new_cluster = i;
            fat[entry_index] = 0x0FFFFFFF;
            fat_cache_dirty = 1;
            fat_cache_flush();  // sofort schreiben

            // Neuen Cluster nullen
            static uint16_t zero[256] = {0};
            uint32_t new_sektor = data_start + (new_cluster - 2) * boot.sectors_per_cluster;
            for(int s = 0; s < boot.sectors_per_cluster; s++)
                ata_write_sector(new_sektor + s, zero);

            break;
        }
    }

    if(new_cluster == 0) {
        kprintf("fat_extend_chain: Kein freier Cluster!\n");
        return 0x0FFFFFFF;
    }

    // prev_cluster auf new_cluster zeigen lassen
    uint32_t prev_sector = fat_start + (prev_cluster * 4) / 512;
    uint32_t prev_index  = (prev_cluster * 4 % 512) / 4;

    if(prev_sector != cached_fat_sector_nr) {
        fat_cache_flush();
        cached_fat_sector_nr = prev_sector;
        ata_read_sector(prev_sector, cached_fat_buf);
    }
    uint32_t *prev_fat = (uint32_t*)cached_fat_buf;
    prev_fat[prev_index] = new_cluster;
    fat_cache_dirty = 1;
    fat_cache_flush();

    return new_cluster;
}

void fat_init_dir_cluster(uint32_t cluster, uint32_t parent_cluster) {
    uint16_t buf[256] = {0};

    FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;
    for(int j = 0; j < 8; j++) eintraege[0].name[j] = ' ';
    for(int j = 0; j < 3; j++) eintraege[0].ext[j]  = ' ';

    eintraege[0].name[0] = '.';
    eintraege[0].attributes = 0x10;
    eintraege[0].cluster_high = cluster >> 16;     // nicht nur cluster
    eintraege[0].cluster_low  = cluster & 0xFFFF;  // fehlte
    
    for(int j = 0; j < 8; j++) eintraege[1].name[j] = ' ';
    for(int j = 0; j < 3; j++) eintraege[1].ext[j]  = ' ';

    eintraege[1].name[0] = '.';
    eintraege[1].name[1] = '.';
    eintraege[1].attributes = 0x10;
    eintraege[1].cluster_high = parent_cluster >> 16;   // obere 16 Bits
    eintraege[1].cluster_low  = parent_cluster & 0xFFFF;
    
    uint32_t sektor = data_start + (cluster - 2) * boot.sectors_per_cluster;
    ata_write_sector(sektor, (uint16_t*)buf);
    
}

void fat_open_dir(char name[8]) {
    uint16_t buf[256];                          // Puffer für einen Sektor
    uint32_t current_cluster = cwd_cluster;  
    int foundFolder = 0;
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;

        for(int s = 0; s < boot.sectors_per_cluster;s++) {
            ata_read_sector(sektor + s, buf);               // Sektor in buf laden
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;
            for(int i = 0; i < 16; i++) {
                //hier jetzt checken  if einträge gleich ordner 

                
                if(eintraege[i].attributes == 0x10 && str_equal_fat((char*)eintraege[i].name, name)) {
                    cwd_cluster = ((uint32_t)eintraege[i].cluster_high << 16) | eintraege[i].cluster_low;
                    str_end(name);
                    foundFolder = 1;
                    break;
                }
            }
        } 
    current_cluster = fat_next_cluster(current_cluster);
    if(foundFolder) break;     
    }
    if(!foundFolder) kprintf("Ordner nicht gefunden: %s\n", name);
}




// ============================================================
// fat_cd_root() wechselt eifnahc in das root verzeichnis
// ============================================================

void fat_cd_root() {
    cwd_cluster = root_cluster;
    cwd_path[0] = '\0';
}

// ============================================================
// fat_cd_back() geht ein ordner zurück also von home/user zu home/
// ============================================================
void fat_cd_back() {
    if(cwd_cluster == root_cluster) {
        kprintf("Bereits im root Verzeichnis\n");
        return;
    }
    static uint16_t buf[256];
    uint32_t sektor = data_start + (cwd_cluster - 2) * boot.sectors_per_cluster;
    
    ata_read_sector(sektor, buf);
    FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;

    uint32_t parent = ((uint32_t)eintraege[1].cluster_high << 16) | eintraege[1].cluster_low;

    
    if(parent == 0) parent = root_cluster;
    cwd_cluster = parent;
    int i = 0;
    while(cwd_path[i] != '\0') i++;
    if(i > 0) i--;                              // skip '\0'
    if(i > 0 && cwd_path[i] == '/') i--;       // skip trailing '/'
    while(i > 0 && cwd_path[i] != '/') i--;    // find last '/'
    cwd_path[i + 1] = '\0';                    
}


// ============================================================
// Löscht das ausgewählte file
// ============================================================

void fat_delete_file(char *arg2) {

    static uint16_t buf[256];
    static uint16_t fbuf[256];
    uint32_t current_cluster = cwd_cluster;
    uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    int foundFile = 0;

    while (1) {

        for(int i = 0; i < boot.sectors_per_cluster && !foundFile;i++) {       //geht solang durch alles durch bis file gefunden wurde
            ata_read_sector(sektor + i, buf);
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;
            for(int j = 0; j <16; j++) {


                if(str_equal_fat((char*)eintraege[j].name,arg2)) {
                    uint32_t file_cluster = ((uint32_t)eintraege[j].cluster_high << 16) | eintraege[j].cluster_low;
                    eintraege[j].name[0] =  0xE5;          //auf gelöscht setzen indem erste byte des namens 0xE5 ist
                    foundFile = 1;
                    ata_write_sector(sektor + i, buf);
                    while (file_cluster < 0x0FFFFFF8) {
                        uint32_t next = fat_next_cluster(file_cluster);
                        uint32_t fat_sector  = fat_start + (file_cluster * 4) / 512;
                        uint32_t entry_index_old = (file_cluster * 4 % 512) / 4;
                        ata_read_sector(fat_sector, fbuf);
                        uint32_t *fat = (uint32_t*)fbuf;
                        fat[entry_index_old] = 0x00000000;
                        ata_write_sector(fat_sector, fbuf);
                        file_cluster = next;  
                    }
                }
            }

        }
        if(foundFile) break;
        uint32_t next = fat_next_cluster(current_cluster);
        if(next >= 0x0FFFFFF8) {
            kprintf("Datei nicht gefunden :(\n");
            break;
        }
        current_cluster = next;
        sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    }
}



// ============================================================
// Rename flag ändert den namen einer alten datei
// ============================================================
void fat_rename_file(char *arg,char *arg2) {

    uint32_t current_cluster = cwd_cluster;
    uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    static uint16_t buf[256];
    int foundFile = 0;

    while(1) {
        for(uint32_t s = 0;s < boot.sectors_per_cluster && !foundFile;s++) {
            ata_read_sector(sektor + s, buf);               // Sektor in buf laden
            FAT32DirEntry *eintraege = (FAT32DirEntry*)buf;
            for(int i = 0; i < 16; i++) {
                if(str_equal_fat((char*)eintraege[i].name,arg)) {
                for (int j = 0; j < 8;j++) eintraege[i].name[j] = ' ';                           //erst namen leeren
                for (int j = 0; j < 8 && arg2[j] != '\0';j++) eintraege[i].name[j]  = arg2[j];  //dann mit neuen namen befüllen
                ata_write_sector(sektor + s,buf);
                foundFile = 1;
                break;
                }
            }
        }
    if(foundFile) break;
    uint32_t next = fat_next_cluster(current_cluster);
    if(next >= 0x0FFFFFF8) { kprintf("nicht gefunden :(n"); break; }
    current_cluster = next;
    sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    }

}


// ============================================================
// File bearbeiten ganzer code editor wir dabei gebaut
// ============================================================


void fat_read_file(char *name, uint8_t *buf, uint32_t *size) {

    uint32_t offset = 0;
    // kprintf("DEBUG: fat_read_file start, name=%s\n", name);

    uint32_t current_cluster = cwd_cluster;

    // kprintf("DEBUG: cwd_cluster=%u\n", current_cluster);

    uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    // kprintf("DEBUG: sektor=%u\n", sektor);
    static uint16_t sektorbuf[256];
    int foundFile = 0;
    uint32_t eintrag = 0;

    while(1){
        for(uint32_t s = 0;s < boot.sectors_per_cluster && !foundFile;s++) {
            // kprintf("DEBUG: vor ata_read_sector\n");
            ata_read_sector(sektor + s, sektorbuf);               // Sektor in buf laden
            FAT32DirEntry *eintraege = (FAT32DirEntry*)sektorbuf;
            for(int i = 0; i < 16;i++) {
                if(str_equal_fat((char*)eintraege[i].name,name)) {
                    foundFile = 1;
                    eintrag = i;
                    break;
                }

            }
        }
        if(foundFile) break; 
    uint32_t next = fat_next_cluster(current_cluster);
    if(next >= 0x0FFFFFF8) { kprintf("nicht gefunden\n"); break; }
    current_cluster = next;
    sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    }
    if (!foundFile) { *size = 0; return; }
        // Datei gefunden - Cluster und Size aus Dir-Eintrag holen
    FAT32DirEntry *eintraege = (FAT32DirEntry*)sektorbuf;
    uint32_t file_cluster = ((uint32_t)eintraege[eintrag].cluster_high << 16) 
                                    | eintraege[eintrag].cluster_low;
    *size = eintraege[eintrag].size;

    if(*size == 0) return;  // leere Datei, nichts zu lesen 
    // Cluster für Cluster in buf lesen
    while(file_cluster < 0x0FFFFFF8) {
        uint32_t file_sektor = data_start + (file_cluster - 2) * boot.sectors_per_cluster;
        for(int s = 0; s < boot.sectors_per_cluster; s++) {
            ata_read_sector(file_sektor + s, sektorbuf);
            for(int j = 0; j < 512; j++) {
                buf[offset + j] = ((uint8_t*)sektorbuf)[j];
            }
            offset += 512;
        }
        file_cluster = fat_next_cluster(file_cluster);
    }
}





void fat_write_data(char *name, uint8_t *buf,uint32_t size) {
    uint32_t current_cluster = cwd_cluster;
    uint32_t sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    static uint16_t sektorbuf[256];
    int foundFile = 0;
    uint32_t file_cluster = 0;

    while (1) {
        for(uint32_t s = 0;s < boot.sectors_per_cluster && !foundFile;s++) {
            ata_read_sector(sektor + s,sektorbuf);
            FAT32DirEntry *eintraege = (FAT32DirEntry*)sektorbuf;
            for(int i = 0; i < 16;i++){
                    if(str_equal_fat((char*)eintraege[i].name,name)) {         //checkt ob das die datei die datei ist die wir wollen
                    eintraege[i].size = size;                       //Size updaten von der datei 
                    file_cluster = ((uint32_t)eintraege[i].cluster_high) << 16 | eintraege[i].cluster_low;
                    //              als uint32 casten weil << 16 heißt 16 bits nach oben und cluster_high ist ein 
                    //uint16_t was bedeutet man muss es auf uint32_t erweitern sonst wären die cluster_high sachen müll irgendwo
                    ata_write_sector(sektor + s, sektorbuf);
                    foundFile = 1;
                    break;
                }
            }
        }
        //wenn im jetzigen cluster die datei nicht liegt dann in den nächsten cluster gehen
        if(foundFile) break;
    uint32_t next = fat_next_cluster(current_cluster);
    if(next >= 0x0FFFFFF8) { kprintf("Nicht genügend Speicherplatz!\n"); break; }
    current_cluster = next;
    sektor = data_start + (current_cluster - 2) * boot.sectors_per_cluster;
    }

    

    uint32_t offset = 0;
    while(file_cluster < 0x0FFFFFF8) {
        uint32_t file_sektor = data_start + (file_cluster - 2) * boot.sectors_per_cluster;
        for(uint32_t s = 0; s < boot.sectors_per_cluster;s++) {
            if (offset >= size) break;
            // buf uint8 in sektorbuf uint16 kopieren 
            for(int j = 0; j < 512;j++) {
                // wir wollen buf in sektorbuf kopieren aber das es uint16 ist machen wir 
                // das byte für byte weise 
                // hatte das als if aber claude hat gesagt das ich das so mega krass kompremieren kann 
                ((uint8_t*)sektorbuf)[j] = (offset + j < size) ? buf[offset + j] : 0;
            }
            ata_write_sector(file_sektor + s,sektorbuf);
            offset += 512;
        }
    if(offset < size) {  
        // offset sind die bytes die sektoren die wir schon gespeihcert haben in bytes
        // daten und wenn wir immer noch mehr daten speichern wolle extenden 
        //wie einfach den file_cluster bis wir keine daten  mehr haben
        
        file_cluster = fat_extend_chain(file_cluster);
    } else {
        break;      //wenn  keine daten mehr einfach abbrechen
    }   
    }
}




int str_equal_fat(char *fat_name, char *name) {
    int i = 0;
    while(i < 8) {
        if(fat_name[i] == ' ' && name[i] == '\0') return 1;  // beide enden hier
        if(fat_name[i] != name[i]) return 0;                  // unterschiedlich
        i++;
    }
    return name[i] == '\0';  // name muss auch zuende sein
}

//looks for the end of a array end extends cwd_path wenn a folder is opend
void str_end(char *name) {
    for(int i = 0; i < 256;i++) {
    if(cwd_path[i] == '\0') {
        int j = 0;
        while(name[j] != '\0') {
            cwd_path[i] = name[j];  // Zeichen reinschreiben
            i++;
            j++;
            }
        cwd_path[i] = '/';
        i++;
        cwd_path[i] = '\0';
        break;
        }   
    }
}



//Gets the cwd_path 
char* fat_get_cwdpath(){
    return cwd_path;
}  
