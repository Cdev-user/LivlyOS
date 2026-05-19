// stress_test.c
#include "stress_test.h"
#include "../mm/pmm.h"
#include "../mm/kmalloc.h"
#include "console.h"

// ── Hilfsfunktion: Zahl ausgeben ohne kprintf ──────────────────
// (falls dein kprintf noch nicht 100% stabil ist)
static void print_dec(uint64_t n) {
    if (n == 0) { kprintf("0"); return; }
    char buf[20];
    int  i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--) kprintf("%c", buf[j]);
}

// ═══════════════════════════════════════════════════════════════
// PMM Stress-Test
// Prüft: Alloc/Free korrekt, kein Underflow, kein Bitmap-Overflow
// ═══════════════════════════════════════════════════════════════
void pmm_stress_test() {
    kprintf("\n=== PMM Stress Test ===\n");

    uint64_t free_vorher = pmm_get_free_pages();
    kprintf("Freie Pages am Start: "); print_dec(free_vorher); kprintf("\n");

    // ── Test 1: 16 Pages allozieren ───────────────────────────
    kprintf("[1] 16 Pages allozieren... ");
    uint64_t pages[16];
    int ok = 1;

    for (int i = 0; i < 16; i++) {
        pages[i] = pmm_alloc();
        if (pages[i] == 0xFFFFFFFFFFFFFFFF) {
            kprintf("FEHLER bei i="); print_dec(i); kprintf("\n");
            ok = 0;
        }
    }
    if (ok) kprintf("OK\n");

    uint64_t free_nach_alloc = pmm_get_free_pages();
    kprintf("   Freie Pages jetzt: "); print_dec(free_nach_alloc); kprintf("\n");

    // Genau 16 weniger?
    if (free_vorher - free_nach_alloc == 16)
        kprintf("   Zähler: OK (-16)\n");
    else {
        kprintf("   Zähler: FEHLER! Erwartet -16, got ");
        print_dec(free_vorher - free_nach_alloc);
        kprintf("\n");
    }

    // ── Test 2: Alle 16 Pages freigeben ───────────────────────
    kprintf("[2] 16 Pages freigeben... ");
    for (int i = 0; i < 16; i++) pmm_free(pages[i]);

    uint64_t free_nach_free = pmm_get_free_pages();
    if (free_nach_free == free_vorher)
        kprintf("OK (Zähler erholt)\n");
    else {
        kprintf("FEHLER! Vorher="); print_dec(free_vorher);
        kprintf(" Nachher=");       print_dec(free_nach_free);
        kprintf("\n");
    }

    // ── Test 3: Doppeltes Free (darf NICHT crashen) ───────────
    kprintf("[3] Double-Free Test... ");
    pmm_free(pages[0]);   // schon frei
    pmm_free(pages[0]);   // nochmal → muss silent ignoriert werden
    if (pmm_get_free_pages() == free_vorher)
        kprintf("OK (kein Underflow)\n");
    else {
        kprintf("FEHLER! free_pages="); print_dec(pmm_get_free_pages()); kprintf("\n");
    }

    // ── Test 4: Doppeltes Alloc derselben Adresse unmöglich ───
    kprintf("[4] Keine Doppel-Allokation... ");
    uint64_t a = pmm_alloc();
    uint64_t b = pmm_alloc();
    if (a != b)
        kprintf("OK (verschiedene Adressen)\n");
    else
        kprintf("FEHLER! Beide Adressen gleich: "); // print_dec(a);
    pmm_free(a);
    pmm_free(b);

    // ── Test 5: Canary – Bitmap-Overflow-Detektor ─────────────
    // Schreib einen bekannten Wert direkt hinter die Bitmap.
    // Falls pmm irgendetwas dahinter schreibt → Canary ändert sich.
    kprintf("[5] Bitmap-Overflow Canary... ");
    uint8_t *canary_ptr = (uint8_t *)pmm_bitmap_end;   // erstes Byte des Heaps
    *canary_ptr = 0xAB;                                 // bekannter Wert

    // Ein paar Allocs machen die nahe an der Bitmap-Grenze wären
    uint64_t tmp[8];
    for (int i = 0; i < 8; i++) tmp[i] = pmm_alloc();
    for (int i = 0; i < 8; i++) pmm_free(tmp[i]);

    if (*canary_ptr == 0xAB)
        kprintf("OK (0xAB intakt)\n");
    else {
        kprintf("FEHLER! Canary=0x");
        // Hex ausgeben
        uint8_t v = *canary_ptr;
        kprintf("%c", "0123456789ABCDEF"[v >> 4]);
        kprintf("%c", "0123456789ABCDEF"[v & 0xF]);
        kprintf("\n");
    }

    kprintf("=== PMM Test abgeschlossen ===\n\n");
}


// ═══════════════════════════════════════════════════════════════
// Heap Stress-Test
// Prüft: kmalloc/kfree, Splitting, Merging, kein Null-Return
// ═══════════════════════════════════════════════════════════════
void heap_stress_test() {
    kprintf("=== Heap Stress Test ===\n");

    // ── Test 1: Einfaches Alloc/Free ──────────────────────────
    kprintf("[1] Einfaches kmalloc/kfree... ");
    void *p = kmalloc(64);
    if (p == 0) { kprintf("FEHLER: NULL\n"); }
    else {
        // Schreib in den Speicher – crasht bei falschem Pointer sofort
        uint8_t *b = (uint8_t *)p;
        for (int i = 0; i < 64; i++) b[i] = (uint8_t)i;
        // Verifizieren
        int ok = 1;
        for (int i = 0; i < 64; i++) if (b[i] != (uint8_t)i) { ok = 0; break; }
        kprintf(ok ? "OK\n" : "FEHLER: Daten korrupt\n");
        kfree(p);
    }

    // ── Test 2: Mehrere Allocs, dann alle freigeben ───────────
    kprintf("[2] 8x kmalloc dann kfree... ");
    void *ptrs[8];
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        ptrs[i] = kmalloc(128);
        if (ptrs[i] == 0) { ok = 0; break; }
        // direkt reinschreiben
        ((uint8_t*)ptrs[i])[0] = (uint8_t)i;
    }
    for (int i = 0; i < 8; i++) if (ptrs[i]) kfree(ptrs[i]);

    // Danach: kann wieder alloziert werden?
    void *nach = kmalloc(512);
    if (ok && nach != 0) kprintf("OK\n");
    else kprintf("FEHLER: kein Speicher nach free\n");
    kfree(nach);

    // ── Test 3: Merge-Check ───────────────────────────────────
    // Zwei benachbarte Blöcke freigeben → merge → ein großer Block
    kprintf("[3] Block-Merge nach kfree... ");
    void *x = kmalloc(64);
    void *y = kmalloc(64);
    kfree(x);
    kfree(y);
    // Jetzt müsste ein Block mit >= 128+sizeof(Block) Bytes frei sein
    void *gross = kmalloc(128 + sizeof(Block));
    if (gross != 0) kprintf("OK (Merge funktioniert)\n");
    else             kprintf("FEHLER: Merge hat nicht gemergt\n");
    kfree(gross);

    // ── Test 4: NULL-Free (darf nicht crashen) ────────────────
    kprintf("[4] kfree(NULL)... ");
    kfree(0);
    kprintf("OK\n");

    // ── Test 5: Viele kleine Allocs (Fragmentierung) ──────────
    kprintf("[5] 32x kleine Allocs... ");
    void *klein[32];
    ok = 1;
    for (int i = 0; i < 32; i++) {
        klein[i] = kmalloc(16);
        if (klein[i] == 0) { ok = 0; break; }
    }
    // Jeden zweiten freigeben → Schachbrett-Fragmentierung
    for (int i = 0; i < 32; i += 2) kfree(klein[i]);
    // Rest freigeben
    for (int i = 1; i < 32; i += 2) kfree(klein[i]);
    // Danach: großer Block muss wieder allozierbar sein
    void *defrag = kmalloc(256);
    if (ok && defrag != 0) kprintf("OK\n");
    else kprintf("FEHLER: Fragmentierung nicht aufgeloest\n");
    kfree(defrag);

    kprintf("=== Heap Test abgeschlossen ===\n\n");
}