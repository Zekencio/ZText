#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

// Constants and macros will be put here if needed

struct termios og_termios;

// Function prototyping

void enableRawInput();
void disableRawInput();
void printEditorError(const char *s);

// Main function (entry point)

int main(){

    enableRawInput();

    while(1){
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN){
            printEditorError("Read error");
        }

        if(iscntrl(c)){
            printf("%d\r\n", c);
        }else{
            printf("%d ('%c') \r\n", c, c);
        }
        if(c == 'q') break;
    }

    return EXIT_SUCCESS;
}

// Custom functions

void enableRawInput(){

    if(tcgetattr(STDIN_FILENO, &og_termios) == -1){
        printEditorError("Get terminal attributes upon enabling raw mode error");
    }
    atexit(disableRawInput);

    struct termios raw = og_termios;

    // Disabling all posible flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);    
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
        printEditorError("Set terminal attributes upon enabling raw mode error");
    }
}

void disableRawInput(){
    // If this messes up, we're in deep shit
    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &og_termios) == -1)
    {
        printEditorError("Set terminal attributes upon disabling raw mode error");
    }
}

// *pain.jpg*
void printEditorError(const char *s){
    perror(s);
    exit(1);
}