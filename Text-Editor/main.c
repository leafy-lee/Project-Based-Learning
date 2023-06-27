#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

struct termios orig_termios;

void print_binary(unsigned int number) {
    if (number >> 1) {
        print_binary(number >> 1);
    }
    putc((number & 1) ? '1' : '0', stdout);
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    // ICRNL disable “\n“ to "\r\n" | IXON disable ctrl-S(XOFF) and ctrl-Q(XON)
    // BRKINT, INPCK, ISTRIP, and CS8 
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

void die(const char *s){
    perror(s);
    exit(1);
}

int main(){
    enableRawMode();

    while (1) {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }

    return 0;
}