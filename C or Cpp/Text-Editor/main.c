/*** includes ***/

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <string.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

#define DEBUG 0

/*** data ***/

struct editorConfig {  
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
    // perror() comes from <stdio.h>, and exit() comes from <stdlib.h>
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);

    printf("\r");

    exit(1);
}

void print_binary(unsigned int number) {
    if (number >> 1) {
        print_binary(number >> 1);
    }
    putc((number & 1) ? '1' : '0', stdout);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    // atexit() comes from <stdlib.h>
    atexit(disableRawMode);

    // all comes from <termios.h>
    struct termios raw = E.orig_termios;
    // ICRNL disable “\n“ to "\r\n" | IXON disable ctrl-S(XOFF) and ctrl-Q(XON)
    // either already turned off, or don’t really apply to modern terminal emulators.
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // disable “\n” to "\r\n" post-processing of output
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    // disble output of exhibit     | disable Canonical mode (read byte-by-byter)
    // diable ctrl-V                | disable ctrl-C(SIGINT) and ctrl-Z(SIGTSTP)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // minimum number of bytes of input needed before read() can return.
    raw.c_cc[VMIN] = 0;
    // maximum amount of time to wait before read() returns.
    raw.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    // sscanf() comes from <stdio.h>.
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // ioctl(), TIOCGWINSZ, and struct winsize come from <sys/ioctl.h>.
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (DEBUG) printf("[DEBUG]: getWindowSize - if\n");
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        // if (DEBUG) printf("<\n");
        // editorReadKey();
        return getCursorPosition(rows, cols);
        editorReadKey();
        return -1;
    } else {
        if (DEBUG) printf("[DEBUG]: getWindowSize - else\n");
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        if (DEBUG) printf("[DEBUG]: cols:%d, rows:%d\n", ws.ws_col, ws.ws_row);
        return 0;
    }
}

/*** append buffer ***/

struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    // realloc() and free() come from <stdlib.h>. memcpy() comes from <string.h>.
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/ 

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        // snprintf() comes from <stdio.h>.
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
             "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
            abAppend(ab, "~", 1);
            padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
            if (DEBUG){
                int padding2 = (E.screencols - welcomelen) / 2;
                while (padding2--) abAppend(ab, "-", 1);
                printf("__%d;%d-%d  ", E.screenrows / 3, E.screencols, welcomelen);
            }
        } else {
            if (DEBUG){
                int pad = E.screencols;
                while (pad--) abAppend(ab, "~", 1);
            } else {
                abAppend(ab, "~", 1);
            }
        }

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // // write() and STDOUT_FILENO come from <unistd.h>.
    // // https://vt100.net/docs/vt100-ug/chapter3.html#ED
    // // ncurses uses the terminfo database to decide escape sequences.
    // write(STDOUT_FILENO, "\x1b[2J", 4);
    // write(STDOUT_FILENO, "\x1b[H", 3);

    abAppend(&ab, "\x1b[?25l", 6);
    // abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    // write(STDOUT_FILENO, "\x1b[H", 3);
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25l", 6);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** init ***/

void initEditor() {
    // if (getWindowSizeSoft(&E.screencols, &E.screenrows) == -1) die("getWindowSizeSoft");
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    if (DEBUG){
        printf("%d;%d", E.screenrows, E.screencols);
    }
}

int main(){
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}