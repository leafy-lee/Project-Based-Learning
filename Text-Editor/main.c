/*** includes ***/

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
    // perror() comes from <stdio.h>, and exit() comes from <stdlib.h>
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void print_binary(unsigned int number) {
    if (number >> 1) {
        print_binary(number >> 1);
    }
    putc((number & 1) ? '1' : '0', stdout);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    // atexit() comes from <stdlib.h>
    atexit(disableRawMode);

    // all comes from <termios.h>
    struct termios raw = orig_termios;
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

/*** output ***/ 

void editorDrawRows() {
    int y;
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {
    // write() and STDOUT_FILENO come from <unistd.h>.
    // https://vt100.net/docs/vt100-ug/chapter3.html#ED
    // ncurses uses the terminfo database to decide escape sequences.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        // write(STDOUT_FILENO, "\x1b[2J", 4);
        // write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** init ***/

int main(){
    enableRawMode();

    while (1) {
        editorProcessKeypress();
    }

    return 0;
}