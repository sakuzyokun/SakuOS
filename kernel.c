#define WIDTH  80
#define HEIGHT 25
#define PIT_FREQ 100
#define HISTORY_MAX 16
#define RAMFS_MAX_FILES 32
#define RAMFS_MAX_NAME  16
#define RAMFS_MAX_SIZE  512
#define NULL ((void*)0)

// ===== グローバル変数 =====
char screen[HEIGHT][WIDTH];
int cursor_pos = 0;
int shift = 0;
int caps = 0;
int cursor_x = 0;
int cursor_y = 0;
volatile int ticks = 0;
volatile int cursor_visible = 1;
char input[128];
int input_len = 0;
char history[HISTORY_MAX][128];
int history_count = 0;
int history_index = 0;
char saved_char;
int current_dir = 1; // root
int mouse_x = 160;
int mouse_y = 100;
int mouse_buttons = 0;

// ===== Window =====
typedef struct {
    int x, y;
    int w, h;
    char title[32];
    int focused;

    int minimized;
    int maximized;

    // 元のサイズ
    int old_x, old_y;
    int old_w, old_h;

    // ★ コンテンツ（テキスト）
    char content[256];

    // ★ ボタン
    int has_button;
    int button_x, button_y;
    int button_w;
    char button_label[16];
} window_t;

#define MAX_WINDOWS 8
window_t windows[MAX_WINDOWS];
int window_count = 0;
int focused_index = 0;

// ===== RamFS 宣言 =====
typedef struct {
    char name[32];
    char data[1024];
    int size;
    int used;
    int is_dir; // ★ 追加：ディレクトリかどうか
    int parent; // ★ 追加：親ディレクトリの index
} ramfs_file_t;

#define RAMFS_MAX RAMFS_MAX_FILES

//extern ramfs_file_t ramfs[RAMFS_MAX];
ramfs_file_t ramfs[RAMFS_MAX];

//int ramfs_find(const char* name);

typedef enum {
    OUTPUT_CONSOLE,
    OUTPUT_FILE
} output_mode_t;

output_mode_t output_mode = OUTPUT_CONSOLE;
char* output_file = NULL;

void outb(unsigned short port, unsigned char value);

void pic_eoi(unsigned char irq) {
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

void pit_init() {
    unsigned int divisor = 1193180 / PIT_FREQ;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void pit_handler() {
    ticks++;
    if (ticks % 50 == 0) {   // 0.5秒ごと
        cursor_visible ^= 1;
    }
    pic_eoi(0);
}

int strlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

void strcpy(char* dst, const char* src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = 0;
}

int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i] || !b[i])
            return a[i] - b[i];
    }
    return 0;
}

void itoa(int n, char* buf) {
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    buf[i] = 0;

    // reverse
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = t;
    }
}

void scroll_up() {
    for (int y = 1; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            screen[y - 1][x] = screen[y][x];

    for (int x = 0; x < WIDTH; x++)
        screen[HEIGHT - 1][x] = ' ';
}

void vga_putc(char c) {
    char* vga = (char*)0xB8000;
    int pos = (cursor_y * WIDTH + cursor_x) * 2;

    vga[pos] = c;
    vga[pos + 1] = 0x07;
}

int ramfs_find_in(const char* name, int parent);

void ramfs_append(const char* name, char c) {
    int idx = ramfs_find_in(name, current_dir);
    if (idx < 0) return;

    ramfs_file_t* f = &ramfs[idx];

    if (f->is_dir) return; // ディレクトリに書き込まない

    if (f->size < RAMFS_MAX_SIZE - 1) {
        f->data[f->size++] = c;
        f->data[f->size] = 0;
    }
}

void put_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= HEIGHT) {
            scroll_up();
            cursor_y = HEIGHT - 1;
        }
    return;
    }

    screen[cursor_y][cursor_x] = c;
    cursor_x++;

    if (cursor_x >= WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= HEIGHT) {
        scroll_up();
        cursor_y = HEIGHT - 1;
    }

    if (output_mode == OUTPUT_CONSOLE) {
        vga_putc(c);
    } else {
        ramfs_append(output_file, c);
    }
}

void puts(const char* s) {
    while (*s) {
        put_char(*s++);
    }
    put_char('\n');
}

void kputs(const char* s) {
    while (*s) put_char(*s++);
    put_char('\n');
}

void kputs_inline(const char* s) {
    while (*s) put_char(*s++);
}

void cls() {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            screen[y][x] = ' ';
    cursor_x = 0;
    cursor_y = 0;
}

void cls_line() {
    cursor_x = 2; // "> " の後
    for (int x = cursor_x; x < WIDTH; x++) {
        screen[cursor_y][x] = ' ';
    }
}

void draw_cursor() {
    saved_char = screen[cursor_y][cursor_x];
    screen[cursor_y][cursor_x] = '_';
}

void erase_cursor() {
    screen[cursor_y][cursor_x] = saved_char;
}

void redraw() {
    char* vga = (char*)0xB8000;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int i = (y * WIDTH + x) * 2;
            vga[i] = screen[y][x] ? screen[y][x] : ' ';
            vga[i + 1] = 0x07;
        }
    }
}

char* strtok(char* str, const char* delim) {
    static char* next;
    if (str) next = str;
    if (!next) return NULL;

    while (*next == *delim) next++;
    if (*next == 0) return NULL;

    char* start = next;
    while (*next && *next != *delim) next++;
    if (*next) *next++ = 0;

    return start;
}

char* strchr(const char* s, char c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return NULL;
}

void ramfs_init() {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        ramfs[i].used = 0;

    // root directory
    strcpy(ramfs[0].name, "/");
    ramfs[0].used = 1;
    ramfs[0].is_dir = 1;
    ramfs[0].parent = 0; // root の親は root
    ramfs[0].size = 0;

    current_dir = 0;

    // 初期ファイル
    strcpy(ramfs[1].name, "README.TXT");
    strcpy(ramfs[1].data, "Welcome to SakuOS!\nRAM filesystem active.");
    ramfs[1].size = strlen(ramfs[1].data);
    ramfs[1].used = 1;
    ramfs[1].is_dir = 0;
    ramfs[1].parent = 0; // root の中にある
}
/*
int ramfs_find(const char* name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs[i].used && !strcmp(ramfs[i].name, name))
            return i;
    }
    return -1;
}
*/
int ramfs_find_in(const char* name, int parent) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!ramfs[i].used) continue;
        if (ramfs[i].parent != parent) continue;
        if (!strcmp(ramfs[i].name, name))
            return i;
    }
    return -1;
}

int ramfs_create(const char* name) {
    if (ramfs_find_in(name, current_dir) >= 0)
        return -1;

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!ramfs[i].used) {
            strcpy(ramfs[i].name, name);
            ramfs[i].size = 0;
            ramfs[i].data[0] = 0;
            ramfs[i].used = 1;
            ramfs[i].is_dir = 0;
            ramfs[i].parent = current_dir;
            return 0;
        }
    }
    return -1;
}

void ramfs_clear(const char* name) {
    int i = ramfs_find_in(name, current_dir);
    if (i < 0) return;

    // ディレクトリは clear できない
    if (ramfs[i].is_dir) return;

    ramfs[i].size = 0;
    ramfs[i].data[0] = 0;
}

void shell_redirect_output(char* filename) {
    output_mode = OUTPUT_FILE;
    output_file = filename;
    ramfs_create(filename);
    ramfs_clear(filename);
}

void shell_restore_output() {
    output_mode = OUTPUT_CONSOLE;
    output_file = NULL;
}

int ramfs_create(const char* name);
void ramfs_clear(const char* name);
void ramfs_append(const char* name, char c);

void panic_vga() {
    char* vga = (char*)0xB8000;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int i = (y * WIDTH + x) * 2;
            vga[i] = screen[y][x] ? screen[y][x] : ' ';
            vga[i + 1] = 0x4F;
        }
    }
}

void bsod_vga() {
    char* vga = (char*)0xB8000;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int i = (y * WIDTH + x) * 2;
            vga[i] = screen[y][x] ? screen[y][x] : ' ';
            vga[i + 1] = 0x1F;
        }
    }
}

void boot_animation() {
    cls();
    const char* msg = "SakuOS booting...";
    int y = 12;

    for (int i = 0; msg[i]; i++) {
        screen[y][30 + i] = msg[i];
        redraw();
        for (volatile int d = 0; d < 500000; d++);
    }

    // プログレスバー風
    for (int i = 0; i < 20; i++) {
        screen[y + 2][30 + i] = '#';
        redraw();
        for (volatile int d = 0; d < 300000; d++);
    }
}

void panic(const char* msg) {
    __asm__ volatile ("cli"); // 割り込み禁止

    cls();
    cursor_x = 0;
    cursor_y = 0;
    kputs("!!! KERNEL PANIC !!!");
    kputs(msg);
    panic_vga();

    while (1) __asm__ volatile ("hlt");
}

void bsod(const char* reason, int code) {
    __asm__ volatile ("cli");

    cls();
    cursor_x = 0;
    cursor_y = 0;
    kputs(":( Your PC ran into a problem");
    kputs("");
    kputs(reason);

    char buf[32];
    itoa(code, buf);
    kputs("");
    kputs("");
    kputs_inline("STOP CODE:");
    kputs_inline(buf);
    bsod_vga();

    while (1) __asm__ volatile ("hlt");
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void error_low(const char* msg) {
    bsod(msg, 0); // コード0 = 軽度
}

void error_high(const char* msg) {
    panic(msg);   // 完全停止
}

unsigned char mouse_packet[3];
int mouse_cycle = 0;

void mouse_poll() {
    if (!(inb(0x64) & 0x01)) return;

    unsigned char data = inb(0x60);

    // mouse bit 立ってなければ捨てる
    if (!(inb(0x64) & 0x20)) return;

    mouse_packet[mouse_cycle++] = data;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        signed char dx = mouse_packet[1];
        signed char dy = mouse_packet[2];

        mouse_x += dx;
        mouse_y -= dy;
    }
}

static void mouse_wait_write() {
    while (inb(0x64) & 0x02); // 書き込み待ち
}

static void mouse_wait_read() {
    while (!(inb(0x64) & 0x01)); // 読み込み待ち
}

static void mouse_write(unsigned char data) {
    mouse_wait_write();
    outb(0x64, 0xD4);      // マウスへ送る
    mouse_wait_write();
    outb(0x60, data);
}

static unsigned char mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init() {
    // --- 1. コマンドバイトを読んで IRQ12 を明示的に無効化 ---
    mouse_wait_write();
    outb(0x64, 0x20);      // read command byte
    mouse_wait_read();
    unsigned char status = inb(0x60);

    status &= ~0x02;       // bit1 = IRQ12 disable
    mouse_wait_write();
    outb(0x64, 0x60);      // write command byte
    mouse_wait_write();
    outb(0x60, status);

    // --- 2. 補助デバイス有効化 ---
    mouse_wait_write();
    outb(0x64, 0xA8);      // enable auxiliary device

    // --- 3. デフォルト設定 & データレポート有効化（※IRQは飛ばない） ---
    mouse_write(0xF6);     // set defaults
    mouse_read();          // ACK 破棄

    mouse_write(0xF4);     // enable data reporting
    mouse_read();          // ACK 破棄

    kputs("Mouse init (polling, IRQ12 disabled).");
}

unsigned char packet[3];
int packet_i = 0;

void mouse_handler(unsigned char data) {
    packet[packet_i++] = data;
    if (packet_i == 3) {
        mouse_x += (char)packet[1];
        mouse_y -= (char)packet[2];
        packet_i = 0;
    }
}

void print_mouse() {
    char buf[32];
    char num[16];

    kputs_inline("Mouse: ");

    itoa(mouse_x, num);
    kputs_inline(num);
    kputs_inline(" ");

    itoa(mouse_y, num);
    kputs_inline(num);
}

/*
void mouse_handler() {
    unsigned char data = inb(0x60);

    mouse_packet[mouse_cycle++] = data;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        signed char dx = (signed char)mouse_packet[1];
        signed char dy = (signed char)mouse_packet[2];

        mouse_x += dx;
        mouse_y -= dy; // 画面座標は上が小さいので逆

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= 319) mouse_x = 319;
        if (mouse_y >= 199) mouse_y = 199;

        mouse_buttons = mouse_packet[0] & 0x07; // 左中右
    }

    pic_eoi(12);
}*/

void keyboard_poll();

int create_window(int x, int y, int w, int h, const char* title) {
    if (window_count >= MAX_WINDOWS) return -1;

    window_t* win = &windows[window_count];
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->focused = 0;

    int i = 0;
    while (title[i] && i < 31) {
        win->title[i] = title[i];
        i++;
    }
    win->title[i] = 0;

    window_count++;
    return window_count - 1;
}

void init_gui() {
    window_count = 0;

    create_window(10, 5, 30, 10, "Main Window");
    create_window(20, 8, 25, 8, "Second Window");

    focused_index = 0;
    windows[0].focused = 1;
}

void bring_to_front(int index) {
    if (index < 0 || index >= window_count) return;

    window_t top = windows[index];

    // index のウィンドウを詰める
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }

    // 一番上に置く
    windows[window_count - 1] = top;

    // 新しいフォーカス位置は末尾
    focused_index = window_count - 1;
}

void focus_next_window() {
    windows[focused_index].focused = 0;

    focused_index++;
    if (focused_index >= window_count)
        focused_index = 0;

    windows[focused_index].focused = 1;

    // フォーカス移動したら最前面へ
    bring_to_front(focused_index);
}

void move_window(window_t* win, int dx, int dy) {
    int nx = win->x + dx;
    int ny = win->y + dy;

    // 画面外に出ないように制限
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx + win->w >= WIDTH)  nx = WIDTH  - win->w - 1;
    if (ny + win->h >= HEIGHT) ny = HEIGHT - win->h - 1;

    win->x = nx;
    win->y = ny;
}

void draw_window_content(window_t* win) {
    int x = win->x + 2;
    int y = win->y + 2;

    for (int i = 0; win->content[i] && y < win->y + win->h - 1; i++) {
        if (win->content[i] == '\n') {
            y++;
            x = win->x + 2;
            continue;
        }
        screen[y][x++] = win->content[i];
        if (x >= win->x + win->w - 2) {
            y++;
            x = win->x + 2;
        }
    }
}

void draw_button(window_t* win) {
    if (!win->has_button) return;

    int bx = win->x + win->button_x;
    int by = win->y + win->button_y;
    int bw = win->button_w;

    // [ OK ]
    screen[by][bx] = '[';
    for (int i = 0; i < bw - 2; i++)
        screen[by][bx + 1 + i] = win->button_label[i];
    screen[by][bx + bw - 1] = ']';
}

void close_window(int index) {
    if (index < 0 || index >= window_count) return;

    // index のウィンドウを削除して詰める
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }

    window_count--;

    // フォーカス調整
    if (window_count == 0) {
        focused_index = -1;
        return;
    }

    if (index >= window_count)
        focused_index = window_count - 1;
    else
        focused_index = index;

    windows[focused_index].focused = 1;
}

void maximize_window(window_t* win) {
    if (win->maximized) return;

    win->old_x = win->x;
    win->old_y = win->y;
    win->old_w = win->w;
    win->old_h = win->h;

    win->x = 0;
    win->y = 0;
    win->w = WIDTH;
    win->h = HEIGHT - 2; // タスクバー分

    win->maximized = 1;
}

void restore_window(window_t* win) {
    if (!win->maximized) return;

    win->x = win->old_x;
    win->y = win->old_y;
    win->w = win->old_w;
    win->h = win->old_h;

    win->maximized = 0;
}

void minimize_window(window_t* win) {
    win->minimized = 1;
}

void draw_taskbar() {
    int y = HEIGHT - 1;

    // 背景
    for (int x = 0; x < WIDTH; x++)
        screen[y][x] = '=';

    // ウィンドウ名を並べる
    int pos = 1;
    for (int i = 0; i < window_count; i++) {
        screen[y][pos++] = '[';

        char* t = windows[i].title;
        for (int j = 0; t[j] && pos < WIDTH - 2; j++)
            screen[y][pos++] = t[j];

        screen[y][pos++] = ']';
        screen[y][pos++] = ' ';
    }
}

void draw_window(window_t* win) {
    int x = win->x;
    int y = win->y;
    int w = win->w;
    int h = win->h;

    char bar = win->focused ? '=' : '-';

    // タイトルバー
    for (int i = 0; i < w; i++)
        screen[y][x + i] = (i == 0 || i == w - 1) ? '+' : bar;

    // × ボタン
    screen[y][x + w - 3] = '[';
    screen[y][x + w - 2] = 'X';
    screen[y][x + w - 1] = ']';

    // タイトル
    for (int i = 0; win->title[i] && i < w - 4; i++)
        screen[y][x + 2 + i] = win->title[i];


    // 本体
    for (int j = 1; j < h; j++) {
        screen[y + j][x] = '|';
        for (int i = 1; i < w - 1; i++)
            screen[y + j][x + i] = ' ';
        screen[y + j][x + w - 1] = '|';
    }

    // 下枠
    for (int i = 0; i < w; i++)
        screen[y + h][x + i] = (i == 0 || i == w - 1) ? '+' : '-';

    draw_window_content(win);
    draw_button(win);
}

void draw_all_windows() {
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].minimized)
            draw_window(&windows[i]);
    }
}

void keyboard_poll_gui() {
    if (!(inb(0x64) & 1)) return;
    unsigned char sc = inb(0x60);

    if (sc & 0x80) return;

    switch (sc) {
        case 0x01: // Esc
            close_window(focused_index);
            break;

        case 0x0F: // Tab
            focus_next_window();
            break;
        
        window_t* win = &windows[focused_index];
        
        case 0x1C: // Enter
            if (win->has_button) { 
                strcpy(win->content, "ボタンが押されました！"); 
            }
            break;

        case 0x3F: maximize_window(win); break; // F5
        case 0x40: restore_window(win); break; // F6
        case 0x41: minimize_window(win); break; // F7
        case 0x48: move_window(&windows[focused_index], 0, -1); break;
        case 0x50: move_window(&windows[focused_index], 0,  1); break;
        case 0x4B: move_window(&windows[focused_index], -1, 0); break;
        case 0x4D: move_window(&windows[focused_index],  1, 0); break;
    }
}

void gui_loop() {
    init_gui();

    int w1 = create_window(10, 5, 40, 12, "Text Window");
    strcpy(windows[w1].content,
        "ｺﾚﾊ SakuOS ﾉﾃｷｽﾄｳｨﾝﾄﾞｳﾃﾞｽ\n"
        "ｷｰﾎﾞｰﾄﾞﾀﾞｹﾃﾞｿｳｻﾃﾞｷﾏｽ!\n"
        "256ｼｮｸGUIﾓﾂｸﾙﾖﾃｲ!"
    );

    int w2 = create_window(20, 10, 30, 8, "Button Window");
    windows[w2].has_button = 1;
    windows[w2].button_x = 5;
    windows[w2].button_y = 4;
    windows[w2].button_w = 6;
    strcpy(windows[w2].button_label, "OK");

    while (1) {
        keyboard_poll_gui();

        cls();
        draw_all_windows();
        draw_taskbar();
        redraw();

        for (volatile int i = 0; i < 50000; i++);
    }
}

char keymap[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

char keymap_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' '
};

void backspace() {
    if (cursor_x > 0) {
        cursor_x--;
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = WIDTH - 1;
    }
    screen[cursor_y][cursor_x] = ' ';
}
void cursor_left() {
    erase_cursor();
    if (cursor_x > 0) cursor_x--;
    draw_cursor();
}

void cursor_right() {
    erase_cursor();
    if (cursor_x < WIDTH - 1) cursor_x++;
    draw_cursor();
}

void cursor_up() {
    if (cursor_y > 0) cursor_y--;
}

void cursor_down() {
    if (cursor_y < HEIGHT - 1) cursor_y++;
}

void scroll_down() {
    for (int y = HEIGHT - 1; y > 0; y--)
        for (int x = 0; x < WIDTH; x++)
            screen[y][x] = screen[y - 1][x];

    for (int x = 0; x < WIDTH; x++)
        screen[0][x] = ' ';
}

/*typedef struct {
    const char* name;
    int size;
} file_t;

file_t files[] = {
    { "README.TXT",  128 },
    { "KERNEL.SYS", 4096 },
    { "CONFIG.CFG",   64 },
};
#define FILE_COUNT (sizeof(files)/sizeof(files[0]))*/

// ===== コマンド関数 宣言 =====
void cmd_help(char* args);
void cmd_cls(char* args);
void cmd_ver(char* args);
void cmd_echo(char* args);
void cmd_panic(char* args);
void cmd_bsod(char* args);
void cmd_reboot(char* args);
void cmd_shutdown(char* args);
void cmd_ls(char* args);
void cmd_dir(char* args);
void cmd_cat(char* args);
void cmd_rm(char* args);
void cmd_edit(char* args);
void cmd_cp(char* args);
void cmd_mv(char* args);
void cmd_touch(char* args);
void cmd_rmdir(char* args);
void cmd_mkdir(char* args);
void cmd_cd(char* args);
void cmd_pwd(char* args);
void cmd_sakugui(char* args);

typedef struct {
    const char* name;
    const char* desc;
    void (*func)(char* args);
} command_t;

command_t commands[] = {
    { "help",     "Show this help",                                 cmd_help     },
    { "cls",      "Clear screen",                                   cmd_cls      },
    { "ver",      "Show OS version",                                cmd_ver      },
    { "echo",     "[TEXT] Print text",                              cmd_echo     },
    { "panic",    "[TEXT (Option)] Trigger kernel panic",           cmd_panic    },
    { "bsod",     "[TEXT (Option)] Trigger blue screen",            cmd_bsod     },
    { "reboot",   "Reboot system",                                  cmd_reboot   },
    { "shutdown", "Power off system",                               cmd_shutdown },
    { "exit",     "Power off system",                               cmd_shutdown },
    { "ls",       "List files",                                     cmd_ls       },
    { "dir",      "List directory (DOS)",                           cmd_dir      },
    { "cat",      "[FILE] Concatenate FILE(s) to standard output.", cmd_cat      },
    { "rm",       "[FILE] Remove the FILE(s).",                     cmd_rm       },
    { "edit",     "[FILE] Edting File",                             cmd_edit     },
    { "cp",       "[SRC] [DST] Copy file",                          cmd_cp       },
    { "mv",       "[SRC] [DST] Move/Rename file",                   cmd_mv       },
    { "touch",    "[FILE] Create empty file",                       cmd_touch    },
    { "mkdir",    "[NAME] Create Directry",                         cmd_mkdir    },
    { "rmdir",    "[NAME] Remove Directry",                         cmd_rmdir    },
    { "cd",       "[NAME] Change Directry",                         cmd_cd       },
    { "pwd",      "Print working directory",                        cmd_pwd      },
    { "sakugui",  "Start SakuOS GUI",                               cmd_sakugui  },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

void cmd_help(char* args) {
    for (int i = 0; i < COMMAND_COUNT; i++) {
        kputs_inline(commands[i].name);
        kputs_inline(" - ");
        kputs(commands[i].desc);
    }
}

void cmd_cls(char* args) {
    cls();
}

void cmd_ver(char* args) {
    kputs("SakuOS b1.0");
}

void cmd_echo(char* args) {
    if (args)
        kputs(args);
}

void cmd_panic(char* args) {
    if (args && args[0]) {
        panic(args);
    } else {
        panic("Manual panic invoked");
    }
}

void cmd_bsod(char* args) {
    if (args && args[0]) {
        bsod(args, 1);
    } else {
        bsod("MANUALLY_INITIATED_CRASH", 1);
    }
}

void cmd_reboot(char* args) {
    kputs("Rebooting...");
    outb(0x64, 0xFE);   // キーボードコントローラ経由リセット
    while (1) __asm__ volatile ("hlt");
}

void outw(unsigned short port, unsigned short value) {
    __asm__ volatile ("outw %0, %1" :: "a"(value), "Nd"(port));
}

void cmd_shutdown(char* args) {
    kputs("Shutting down...");
    cls();
    kputs("It's now safe to turn off your computer.");
    outw(0x604, 0x2000);   // ← 正解
    while (1) __asm__ volatile ("hlt");
}

void cmd_ls(char* args) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs[i].used && ramfs[i].parent == current_dir) {
            kputs(ramfs[i].name);
        }
    }
}

void cmd_dir(char* args) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs[i].used && ramfs[i].parent == current_dir) {

            // ディレクトリは [DIR] 表記
            if (ramfs[i].is_dir) {
                kputs_inline("[DIR] ");
            } else {
                kputs_inline("      ");
            }

            kputs_inline(ramfs[i].name);

            if (!ramfs[i].is_dir) {
                kputs_inline("  ");
                char buf[16];
                itoa(ramfs[i].size, buf);
                kputs_inline(buf);
                kputs(" bytes");
            } else {
                put_char('\n');
            }
        }
    }
}

void cmd_cat(char* args) {
    if (!args || !args[0]) {
        kputs("Usage: cat <file>");
        return;
    }

    int i = ramfs_find_in(args, current_dir);
    if (i < 0) {
        kputs("File not found");
        return;
    }

    kputs(ramfs[i].data);
}

void cmd_rm(char* args) {
    int i = ramfs_find_in(args, current_dir);
    if (i < 0) {
        kputs("File not found");
        return;
    }

    if (ramfs[i].is_dir) {
        kputs("Use rmdir for directories");
        return;
    }

    ramfs[i].used = 0;
}

void cmd_edit(char* args) {
    if (!args || !args[0]) {
        kputs("Usage: edit <file>");
        return;
    }

    int i = ramfs_find_in(args, current_dir);
    if (i >= 0 && ramfs[i].is_dir) {
        kputs("Cannot edit a directory");
        return;
    }

    cls();

    ramfs_create(args);
    cls();
    kputs("[CTRL+Q to exit]");

    char buf[RAMFS_MAX_SIZE];
    int len = 0;

    while (1) {
        if (!(inb(0x64) & 1)) continue;
        unsigned char sc = inb(0x60);

        if (sc & 0x80) continue; // release無視

        // CTRL+Q (scancode: Q=0x10, Ctrl=0x1D)
        static int ctrl = 0;
        if (sc == 0x1D) ctrl = 1;
        if (sc == 0x9D) ctrl = 0;

        if (ctrl && sc == 0x10) { // Ctrl+Q
            ramfs_clear(args);
            for (int i = 0; i < len; i++)
                ramfs_append(args, buf[i]);
            cls();
            return;
        }

        if (sc == 0x0E && len > 0) { // Backspace
            len--;
            backspace();
            continue;
        }

        char c = keymap[sc];
        if (!c) continue;

        if (len < RAMFS_MAX_SIZE - 1) {
            buf[len++] = c;
            put_char(c);
        }
    }
}

void cmd_cp(char* args) {
    char* src = strtok(args, " ");
    char* dst = strtok(NULL, " ");

    if (!src || !dst) {
        kputs("Usage: cp <src> <dst>");
        return;
    }

    int s = ramfs_find_in(src, current_dir);

    if (s < 0) {
        kputs("Source not found");
        return;
    }

    if (ramfs[s].is_dir) {
        kputs("Cannot copy a directory");
        return;
    }

    ramfs_create(dst);
    ramfs_clear(dst);

    for (int i = 0; i < ramfs[s].size; i++) {
        ramfs_append(dst, ramfs[s].data[i]);
    }

    kputs("Copied.");
}

void cmd_mv(char* args) {
    char* src = strtok(args, " ");
    char* dst = strtok(NULL, " ");

    if (!src || !dst) {
        kputs("Usage: mv <src> <dst>");
        return;
    }

    // 現在のディレクトリ内で src を探す
    int s = ramfs_find_in(src, current_dir);
    if (s < 0) {
        kputs("File not found");
        return;
    }

    // ディレクトリはまだ mv 対応しない（後で move 対応する）
    if (ramfs[s].is_dir) {
        kputs("Cannot rename a directory");
        return;
    }

    // 同じディレクトリ内に同名があるかチェック
    if (ramfs_find_in(dst, current_dir) >= 0) {
        kputs("Destination already exists");
        return;
    }

    // 名前変更
    strcpy(ramfs[s].name, dst);

    kputs("Renamed.");
}

void cmd_touch(char* args) {
    if (!args || !args[0]) {
        kputs("Usage: touch <file>");
        return;
    }

    int i = ramfs_find_in(args, current_dir);
    if (i >= 0 && ramfs[i].is_dir) {
        kputs("Cannot touch a directory");
        return;
    }

    if (i < 0) {
        ramfs_create(args);
    } else {
        ramfs_clear(args);
    }

    kputs("Touched.");
}

void cmd_mkdir(char* args) {
    if (!args || !args[0]) {
        kputs("Usage: mkdir <dir>");
        return;
    }

    // 同じディレクトリ内に同名があるかチェック
    if (ramfs_find_in(args, current_dir) >= 0) {
        kputs("Already exists");
        return;
    }

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!ramfs[i].used) {
            strcpy(ramfs[i].name, args);
            ramfs[i].used = 1;
            ramfs[i].is_dir = 1;
            ramfs[i].size = 0;
            ramfs[i].parent = current_dir;
            kputs("Directory created.");
            return;
        }
    }

    kputs("No space left");
}

void cmd_rmdir(char* args) {
    if (!args || !args[0]) {
        kputs("Usage: rmdir <dir>");
        return;
    }

    int i = ramfs_find_in(args, current_dir);
    if (i < 0 || !ramfs[i].is_dir) {
        kputs("Not a directory");
        return;
    }

    // 中身チェック
    for (int j = 0; j < RAMFS_MAX_FILES; j++) {
        if (ramfs[j].used && ramfs[j].parent == i) {
            kputs("Directory not empty");
            return;
        }
    }

    ramfs[i].used = 0;
    kputs("Directory removed.");
}

void cmd_cd(char* args) {
    if (!args || !args[0]) {
        current_dir = 1; // root
        return;
    }

    // 親ディレクトリへ移動
    if (!strcmp(args, "..")) {
        if (current_dir != 0) {
            current_dir = ramfs[current_dir].parent;
        }
        return;
    }

    int i = ramfs_find_in(args, current_dir);
    if (i < 0 || !ramfs[i].is_dir) {
        kputs("Not a directory");
        return;
    }

    current_dir = i;
}

void cmd_pwd(char* args) {
    int path_index = 0;
    char* parts[16];

    int dir = current_dir;

    // root の場合
    if (dir == 0) {
        kputs("/");
        return;
    }

    // 親を辿ってパスを逆順に集める
    while (dir != 0) {
        parts[path_index++] = ramfs[dir].name;
        dir = ramfs[dir].parent;
    }

    // root から順に表示
    kputs_inline("/");
    for (int i = path_index - 1; i >= 0; i--) {
        kputs_inline(parts[i]);
        if (i > 0) kputs_inline("/");
    }
    put_char('\n');
}

void cmd_sakugui(char* args) {
    kputs("Switching to GUI mode...");
    mouse_poll();
    print_mouse();
    //gui_loop();
}

void execute_command(char* cmd, char* args) {
    if (!cmd || !cmd[0]) return;

    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (!strcmp(cmd, commands[i].name)) {
            commands[i].func(args);
            return;
        }
    }

    kputs("Unknown command");
}

void shell_execute(char* line) {
    char* redirect = strchr(line, '>');
    char* outfile = NULL;
    int append = 0;

    if (redirect) {
        if (redirect[1] == '>') {
            append = 1;
            *redirect = 0;
            redirect += 2;
        } else {
            *redirect = 0;
            redirect += 1;
        }

        outfile = redirect;
        while (*outfile == ' ') outfile++;
    }

    char* cmd = strtok(line, " ");
    char* args = strtok(NULL, "");

    /*if (outfile) {
        shell_redirect_output(outfile);
    }*/

    if (outfile) {
        if (!append) {
            shell_redirect_output(outfile); // 上書き
        } else {
            output_mode = OUTPUT_FILE;
            output_file = outfile;
            if (ramfs_find_in(outfile, current_dir) < 0)
                ramfs_create(outfile);
            // clear しない → 追記
        }
    }

    execute_command(cmd, args);

    if (outfile) {
        shell_restore_output();
    }
}

void print_prompt() {
    put_char('>');
    put_char(' ');
}

void keyboard_poll() {
    if (!(inb(0x64) & 1)) return;

    unsigned char sc = inb(0x60);

    // カーソル・スクロール系
    static int e0 = 0;

    if (sc == 0xE0) {
        e0 = 1;
        return;
    }

    // 拡張キー
    if (e0) {
        e0 = 0;

        // 離したイベントは無視
        if (sc & 0x80) return;

        switch (sc) {
            case 0x4B: cursor_left();  return;
            case 0x4D: cursor_right(); return;
            case 0x48: // ↑
                if (history_index > 0) {
                    history_index--;
                    cls_line();              // ← 現在行を消す関数（後述）
                    strcpy(input, history[history_index % HISTORY_MAX]);
                    input_len = strlen(input);
                    kputs_inline(input);      // 改行しない表示
                }
                return;

            case 0x50: // ↓
                if (history_index < history_count - 1) {
                    history_index++;
                    cls_line();
                    strcpy(input, history[history_index % HISTORY_MAX]);
                    input_len = strlen(input);
                    kputs_inline(input);
                }
                return;

            case 0x49: scroll_up();    return;
            case 0x51: scroll_down();  return;
        }
    }

    // 離した
    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == 42 || sc == 54) shift = 0;
        return;
    }

    // 押した
    if (sc == 42 || sc == 54) { shift = 1; return; }
    if (sc == 58) { caps ^= 1; return; }

    // BackSpace
    if (sc == 0x0E) {
        if (input_len > 0) {
            input_len--;
            input[input_len] = 0;
            backspace();
        }
        return;
    }

    char c = (shift ^ caps) ? keymap_shift[sc] : keymap[sc];
    if (!c) return;

    // Enter
    if (c == '\n') {
        erase_cursor();
        input[input_len] = 0;   // 終端

        put_char('\n');
        char* cmd = strtok(input, " ");
        char* args = strtok(NULL, "");
        execute_command(cmd, args);

        if (input_len > 0) {
            strcpy(history[history_count % HISTORY_MAX], input);
            history_count++;
        }
        history_index = history_count;

        input_len = 0;
        print_prompt();
        redraw();
        draw_cursor();
        return;
    }

    // 通常文字
    if (input_len < 127 && c >= 32 && c <= 126) {
        input[input_len++] = c;
        put_char(c);
    }
}

void shell_run() {
    while (1) {
        keyboard_poll();
        redraw();
        if (cursor_visible) draw_cursor();
    }
}

void kernel_main(void) {
    ramfs_init();
    boot_animation();
    
    //mouse_init();

    cls();

    print_prompt();
    redraw();
    shell_run();

    char* vga = (char*)0xB8000;
    int pos = 0;
    int blink = 0;

    while (1) {
        keyboard_poll();
        redraw();

        if (cursor_visible)
            draw_cursor();
    }
}