#ifndef ATA_H
#define ATA_H
#include "../../kernel/kernel.h"


#define FAT32_PARTITION_START 2048  // FAT32 fängt bei Sektor 2048 an

// ATA Ports
#define ATA_DATA        0x1F0               //Daten LEsen/schreiben
#define ATA_ERROR       0x1F1               //Fehler/Features
#define ATA_SECTOR_COUNT 0x1F2              //sektoren anzahl
#define ATA_LBA_LOW     0x1F3               //Lba bits 0-7
#define ATA_LBA_MID     0x1F4               //LBA bits 8-15
#define ATA_LBA_HIGH    0x1F5               //LBA bits 16-23
#define ATA_DRIVE       0x1F6               //Drive Head
#define ATA_STATUS      0x1F7               //Status des drives
#define ATA_COMMAND     0x1F7               //Command

// Status Bits
#define ATA_SR_BSY  0x80  // Disk beschäftigt
#define ATA_SR_DRQ  0x08  // Daten bereit

// Befehle
#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30

// Zwei Funktionen:
void ata_read_sector(uint32_t lba, uint16_t *buf);
void ata_write_sector(uint32_t lba, uint16_t *buf);
void ata_flush();

#endif