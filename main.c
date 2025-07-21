#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

// Constants and macros

#define CTRL_KEY(k) ((k) & 0x1f)

// Data

struct termios og_termios;

// Function prototyping

void enableRawInput();
void disableRawInput();
void printEditorError(const char *s);
char readKey();
void processInputs();

// Main function (entry point)

int main(){

    enableRawInput();

    // Infinite loop. Custom functions will call for the program to exit
    while(1){
        processInputs();
    }

    return EXIT_SUCCESS;
}

// User defined functions

// Terminal functions

char readKey(){

    int nRead;
    char c;

    while((nRead = read(STDIN_FILENO, &c, 1)) != 1){
        if(nRead == -1 && errno != EAGAIN){
            perror("Key reading error");
        }
    }

    return c;
}

void enableRawInput(){

    if(tcgetattr(STDIN_FILENO, &og_termios) == -1){
        printEditorError("Get terminal attributes upon enabling raw mode error");
    }
    atexit(disableRawInput);

    struct termios raw = og_termios;

    // Disabling all posible flags bitwise
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

void printEditorError(const char *s){
    // *pain.jpg*
    perror(s);
    exit(1);
}

// Input functions

void processInputs(){

    char c = readKey();

    switch (c)
    {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}

