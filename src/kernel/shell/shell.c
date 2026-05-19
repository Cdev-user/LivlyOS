#include "../../drivers/keyboard.h"
#include "../../mm/pmm.h"
#include "../../proc/schedular.h"
#include "../io.h"
#include "shell.h"
#include "../../drivers/fs/fat.h"
#include "../stress_test.h"
#include "../../apps/editor/editor.h"
#include "../../mm/kmalloc.h"
#include "../kernel.h"
#include "../console.h"
// Forward Declarations — vor commands[]
void cmd_help(char *arg, char *arg2);
void cmd_clear(char *arg, char *arg2);
void cmd_mem(char *arg, char *arg2);
void cmd_ps(char *arg, char *arg2);
void cmd_reboot(char *arg, char *arg2);
void cmd_stopProcess(char *arg, char *arg2);
void cmd_create_file(char *arg, char *arg2);
void cmd_dir(char *arg, char *arg2);
void cmd_test(char *arg, char *arg2);
void cmd_mkdir(char *arg, char *arg2);
void cmd_cd(char *arg, char *arg2);
void cmd_editor_help();
void cmd_remove_file(char *arg,char *arg2);
void cmd_rename_file(char *arg, char *arg2);
void cmd_memmory_test();
void cmd_cat(char *arg,char *arg2);


// Shell Functions
void str_split(char *input, int index, char *out);
void shell_run();
void shell_execute(char *input);
int str_equal(char *a, char *b);
int str_to_int(char *str);
void str_copy(char *src, char *dst);



typedef struct {
    char *name;           // Befehlsname z.B. "help"
    void (*func)(char *arg, char *arg2);       // Funktion die ausgeführt wird
} Command;



static Command commands[] = {
    {"help",   cmd_help},
    {"clear",  cmd_clear},
    {"mem",    cmd_mem},
    {"ps",     cmd_ps},
    {"reboot", cmd_reboot},
    {"taskkill", cmd_stopProcess},
    {"ls", cmd_dir},
    {"create", cmd_create_file},
    {"test", cmd_test},
    {"mkdir", cmd_mkdir},
    {"cd", cmd_cd},
    {"editor_help",cmd_editor_help},
    {"rm",cmd_remove_file},
    {"mv",cmd_rename_file},
    {"stress_test",cmd_memmory_test},
    {"cat",cmd_cat},
    {NULL, NULL}  // Ende der Liste
};



// Shell history 
#define HISTORY_SIZE 20
static char history[HISTORY_SIZE][MAX_INPUT];
static int history_count = 0;
static int history_pos = 0;




// Shell init (Vorbereiten)
void shell_init() {
    console_clear();
    kprintf("Livly Shell\n");
    kprintf("Type 'help' for commands\n\n");
    shell_run();
}

// Process the input 
// gives it to shell_execute
void shell_run() {
    char input[MAX_INPUT];
    int pos = 0;
    int cur = 0;

    kprintf("livly%s> ", fat_get_cwdpath());
    int prompt_col = console_get_col();
    int prompt_row = console_get_row();

    while(1) {
        uint8_t c = keyboard_read();

        if (c == '\n') {
            input[pos] = 0;
            if(pos > 0) {
                for(int i = 0; i < MAX_INPUT; i++)
                    history[history_count % HISTORY_SIZE][i] = input[i];
                history_count++;
                history_pos = history_count;
            }
            console_putchar('\n');
            shell_execute(input);
            pos = 0; cur = 0;
            kprintf("livly%s> ", fat_get_cwdpath());
            prompt_col = console_get_col();
            prompt_row = console_get_row();

        } else if (c == KEY_UP) {
            if(history_pos > 0 && history_pos > history_count - HISTORY_SIZE) {
                history_pos--;
                for(int i = 0; i < MAX_INPUT; i++)
                    input[i] = history[history_pos % HISTORY_SIZE][i];
                pos = 0;
                while(input[pos] != '\0') pos++;
                cur = pos;
                console_set_pos(prompt_col, prompt_row);
                for(int i = 0; i < pos; i++) console_putchar(input[i]);
                for(int i = pos; i < 60; i++) console_putchar(' ');
                console_set_pos(prompt_col + cur, prompt_row);
            }

        } else if (c == KEY_DOWN) {
            if(history_pos < history_count) {
                history_pos++;
                if(history_pos == history_count) {
                    for(int i = 0; i < MAX_INPUT; i++) input[i] = 0;
                    pos = 0; cur = 0;
                } else {
                    for(int i = 0; i < MAX_INPUT; i++)
                        input[i] = history[history_pos % HISTORY_SIZE][i];
                    pos = 0;
                    while(input[pos] != '\0') pos++;
                    cur = pos;
                }
                console_set_pos(prompt_col, prompt_row);
                for(int i = 0; i < pos; i++) console_putchar(input[i]);
                for(int i = pos; i < 60; i++) console_putchar(' ');
                console_set_pos(prompt_col + cur, prompt_row);
            }

        } else if (c == KEY_LEFT) {
            if (cur > 0) {
                cur--;
                console_set_pos(prompt_col + cur, prompt_row);
            }

        } else if (c == KEY_RIGHT) {
            if (cur < pos) {
                cur++;
                console_set_pos(prompt_col + cur, prompt_row);
            }

        } else if (c == '\b') {
            if (cur > 0) {
                for (int i = cur - 1; i < pos - 1; i++)
                    input[i] = input[i+1];
                pos--; cur--;
                input[pos] = 0;
                console_set_pos(prompt_col, prompt_row);
                for (int i = 0; i < pos; i++) console_putchar(input[i]);
                console_putchar(' ');
                console_set_pos(prompt_col + cur, prompt_row);
            }

        } else if (c >= 0x20 && c < 0x80) {
            if (pos < MAX_INPUT - 1) {
                for (int i = pos; i > cur; i--)
                    input[i] = input[i-1];
                input[cur] = (char)c;
                pos++; cur++;
                console_set_pos(prompt_col + cur - 1, prompt_row);
                for (int i = cur - 1; i < pos; i++)
                    console_putchar(input[i]);
                console_set_pos(prompt_col + cur, prompt_row);
            }
        }
    }
}


void shell_execute(char *input) {
    char cmd[64]  = {0};
    char arg[64]  = {0};
    char arg2[64] = {0};
    char arg3[64] = {0};

    str_split(input, 0, cmd);
    str_split(input, 1, arg);
    str_split(input, 2, arg2);
    str_split(input, 3, arg3);
  

    if(cmd[0] == '/'){
        shell_execute_slash(cmd, arg);
        return;
    }

    for(int i = 0; commands[i].name != NULL; i++) {
        if(str_equal(cmd, commands[i].name)) {
            commands[i].func(arg, arg2);
            return;
        }
    }
    kprintf("Unknown command: %s\n", cmd);
}

//Slach execute
//für ordner öffnen etc.
void shell_execute_slash(char *cmd, char *arg) {
    if(arg[0] == '\0') return;
    if(str_equal(cmd, "/edit")) {
            fat_edit_file(arg);     // /edit
        return;
    }
    kprintf("Unkown command: %s\n",cmd);
}




// Function str_equal
// Usage to check if both string a the same size
int str_equal(char *a, char *b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;  // unterschiedlich
        i++;
    }
    return a[i] == b[i];  // gleich
}



// Function str_split
// Usage splits the word from a input example:

// str_split("create hallo txt", 0) → "create"
// str_split("create hallo txt", 1) → "hallo"
// str_split("create hallo txt", 2) → "txt"

void str_split(char *input, int index, char *out) {
    int word = 0;   // welches wort gerade
    int i    = 0;   // position in input
    int j    = 0;   // position in out

    while(input[i] != 0) {
        if(input[i] == ' ') {
            if(word == index) {
                out[j] = 0;  // fertig
                return;
            }
            word++;          // nächstes wort
            j = 0;           // out buffer zurücksetzen
            i++;
            continue;
        }

        if(word == index) {
            if(j < 63) {
            out[j] = input[i];
            j++;
            }
        }
        i++;
    }
    out[j] = 0;
}



// ------COMMANDS--------


// Cmd Help
void cmd_help(char *arg, char *arg2) {
    kprintf("Commands:\n");
    for(int i = 0; commands[i].name != NULL; i++){
        kprintf("Command %s\n",commands[i].name);
    }

}

// Clear Output
void cmd_clear(char *arg, char *arg2) {
    console_clear();        //Vga datei hat direkten zugriff auf vga buffer also so schneller
}

// Ram statistiken anzeigen
void cmd_mem(char *arg, char *arg2) {
    uint64_t free_mb = pmm_get_free_pages() / 256;
    uint64_t total_mb = pmm_get_total_pages() / 256;
    kprintf("Free: %u\n",free_mb);
    kprintf("Total: %u\n",total_mb);
}

// Alle prozesse bekommen

// state ist enum deswegen muss man seperat checken was für ein state gerade ist
const char *state_name(ProcessState state) {
    if (state == NEW)      return "NEW";
    if (state == READY)    return "READY";
    if (state == RUNNING)  return "RUNNING";
    if (state == BLOCKED)  return "BLOCKED";
    if (state == ZOMBIE)   return "ZOMBIE";
    return "UNKNOWN";
}

// Processe printen
void cmd_ps(char *arg, char *arg2) {
    PCB *head = scheduler_get_head();  // ← head hier definieren
    PCB *current = head;               // ← current = head

if(current == NULL) {
    kprintf("No procces\n");
    return;
}

 kprintf("PID  Name          State\n");
do {
    // Prozess ausgeben
    kprintf("%d  %s          %s\n",current->pid,current->name,state_name(current->state));
    current = current->next;
} while (current != head);

}


// Rebot keyboard controller
void cmd_reboot(char *arg, char *arg2) {
    outb(0x64, 0xFE);  // Keyboard Controller Reset
}


// Process stoppen
void cmd_stopProcess(char *arg, char *arg2) {
    schedular_remove(str_to_int(arg));
}

// Dir
void cmd_dir(char *arg, char *arg2){
    fat_list_dir();
}


// Creating a file
void cmd_create_file(char *arg, char *arg2) {
    if (arg[0] == '\0' || arg2[0] == '\0') {
        kprintf("Usage: create <name> <ext>\n");
        kprintf("Example: create hello txt\n");
        return;
    }
    fat_write_file(arg, arg2);
}


// Test command creates as many data as wanted
void cmd_test(char *arg, char *arg2) {
kprintf("Does nothing");
}

// Function mkdir
// Usgae creating folders
void cmd_mkdir(char *arg, char *arg2) {
    char name[16];
    
    if(arg[0] == '\0') {
        str_copy("default", name);
    } else {
        str_copy(arg, name);
    }
    
    fat_create_dir(name);


    // fat funktion aufrufen und arg mitgeben

}

//Function opens a folder
void cmd_cd(char *arg, char *arg2) {
    if(arg[0] == '\0') {
        fat_cd_root();
    } 
    else if(arg[0] == '.' && arg[1] == '.') {
        fat_cd_back();
    }
    else {
        fat_open_dir(arg);
    }
}

//removes file
void cmd_remove_file(char *arg,char *arg2) {
    if(arg[0] == '\0') {
        kprintf("Usage: rm <fileName>");
        return;
    }
    fat_delete_file(arg);
}

//renames file
void cmd_rename_file(char *arg, char *arg2) {
    fat_rename_file(arg,arg2);
}


//Function all commands for editor
void cmd_editor_help() {
    kprintf("Editor functions: ");
    kprintf("Strg + z back");
    kprintf("Strg + y forwards");
    kprintf("Strg + c to copy");
    kprintf("Strg + v to paste");
    kprintf("Strg + f to equal words");
}



void cmd_memmory_test() {
pmm_stress_test();
heap_stress_test();
}


//cat command
void cmd_cat(char *arg,char *arg2) {
    if(arg[0] == '\0') {
        kprintf("Usage: cat dateiname\n");
    } else {
        uint8_t *buf = kmalloc(EDITOR_MAX_LINES * EDITOR_LINE_LEN);
        uint32_t size = 0;
        fat_read_file(arg, buf, &size);
        if(size == 0) {
            kprintf("Datei leer oder nicht gefunden\n");
        } else {
            for(uint32_t i = 0; i < size; i++)
                kprintf("%c", buf[i]);
            kprintf("\n");
        }
        kfree(buf);
    }
}


void str_copy(char *src, char *dst) {
    int i = 0;
    while(src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';  // String beenden
}

int str_to_int(char *str) {
    uint8_t ans = 0;
    for(int i = 0; str[i] != '\0';i++){
         ans = ans * 10 + (str[i] - '0');
    }
    return ans;
}
