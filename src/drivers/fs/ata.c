#include "../../kernel/kernel.h"
#include "ata.h"
#include "../../kernel/io.h"
#include "../../kernel/timer.h"
#include "../../kernel/console.h"

void ata_read_sector(uint32_t lba, uint16_t *buf) {
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MID,  (lba >> 8)  & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_DRIVE,    0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COMMAND,  ATA_CMD_READ);

    int ATA_BSY_err = 0;
    int ATA_DRQ_err = 0;
    int ATA_FINAL = 0; 
    // 1. Warten bis BSY weg
    uint64_t start = get_ms();
    while(inb(ATA_STATUS) & ATA_SR_BSY) {
        if(get_ms() - start > 5000) { kprintf("ATA BSY timeout!\n"); ATA_BSY_err = 1; return; }
    }
    if(ATA_BSY_err) return;

    // 2. Warten bis DRQ gesetzt (Daten bereit)
    start = get_ms();
    while(!(inb(ATA_STATUS) & ATA_SR_DRQ)) {
        if(get_ms() - start > 5000) { kprintf("ATA DRQ timeout!\n"); ATA_DRQ_err = 1; return; }
    }
    if(ATA_DRQ_err) return;

    // 3. Daten lesen
    for(int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_DATA);
    }

    // 4. Warten bis Drive wirklich fertig
    start = get_ms();
    while(inb(ATA_STATUS) & ATA_SR_BSY) {
        if(get_ms() - start > 5000) { kprintf("ATA final timeout!\n"); ATA_FINAL = 1; return; }
    }
    if(ATA_FINAL) return;
}



void ata_write_sector(uint32_t lba, uint16_t *buf) {
    // Schritt 1: warten bis Drive bereit für Befehl
    while(1) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_BSY) continue;
        if (status & 0x40) break;  // DRDY gesetzt → bereit
    }

    // Schritt 2: Befehl senden
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MID,  (lba >> 8)  & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_DRIVE,    0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COMMAND,  ATA_CMD_WRITE);

    // Schritt 3: warten bis DRQ
    while(1) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_BSY) continue;
        if (status & ATA_SR_DRQ) break;  // kein DRDY hier
    }

    // Schritt 4: Daten schreiben
    for(int i = 0; i < 256; i++)
        outw(ATA_DATA, buf[i]);

    // Schritt 5: warten bis fertig
    while(inb(ATA_STATUS) & ATA_SR_BSY);
}


// flush function
void ata_flush() {
    outb(ATA_COMMAND, 0xE7);
    while(inb(ATA_STATUS) & ATA_SR_BSY);
}