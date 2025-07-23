#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>

// CURRENT STEP (TO DO): 76 (Chapter 4)

// Constants and macros

#define ZTEXT_VERSION "0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DELETE_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

// Data

typedef struct{
    int size;
    char *chars;
} editorRow;

struct config
{
    int cx, cy;
    int rowOffset;
    int columnOffset;
    struct termios og_termios;
    int terminalRows;
    int terminalColumns;
    int numRows;
    editorRow *row;
};

struct config editor;

// Function prototyping

void enableRawInput();
void disableRawInput();
void printEditorError(const char *s);
int readKey();
void processInputs();
void refreshScreen();
void drawRows();
int getWindowSize(int *rows, int *columns);
void initializeEditor();
int getCursorPos(int *rows, int *columns);
void moveCursor(int c);
void editorOpen(char* fileName);
void editorAppendRow(char *s, size_t len);
void editorScroll();

// Main function (entry point)

int main(int argc, char *argv[]){

    enableRawInput();
    initializeEditor();
    if(argc >= 2){
        editorOpen(argv[1]);
    }

    // Infinite loop. Custom functions will call for the program to exit
    while(1){
        refreshScreen();
        processInputs();
    }

    return EXIT_SUCCESS;
}

// User defined functions

// Initialization

void initializeEditor(){
    editor.cx = 0;
    editor.cy = 0;
    editor.rowOffset = 0;
    editor.columnOffset = 0;
    editor.numRows = 0;
    editor.row = NULL;

    if(getWindowSize(&editor.terminalRows, &editor.terminalColumns) == -1){
        printEditorError("Window size error");
    }
}

// File input/output functions

void editorOpen(char* fileName){
    FILE *fp = fopen(fileName, "r");
    if (!fp) printEditorError("Cannot open file");

    char *line = NULL;
    size_t lineCap = 0;
    ssize_t lineLength;

    while((lineLength = getline(&line, &lineCap, fp)) != -1)
    {
        while(lineLength > 0 && (line[lineLength - 1] == '\n' || line[lineLength - 1] == '\r'))
        {
            lineLength--;
        }
        editorAppendRow(line, lineLength);
    }
    free(line);
    fclose(fp);
}

// Terminal functions

int readKey(){

    int nRead;
    char c;

    while((nRead = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nRead == -1 && errno != EAGAIN)
        {
            perror("Key reading error");
        }
    }

    if(c == '\x1b')
    {
        char sequence[3];

        if(read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';

        if(sequence[0] == '[')
        {
            if(sequence[1] >= '0' && sequence[1] <= '9')
            {
                if (read(STDIN_FILENO, &sequence[2], 1) != 1) return '\x1b';
                if (sequence[2] == '~')
                {
                    switch (sequence[1])
                    {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DELETE_KEY;
                        case '4':
                            return END_KEY;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                    }
                }
            }else
            {
                switch (sequence[1])
                {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        }
        return '\x1b';
    }

    return c;

}

void enableRawInput(){

    if(tcgetattr(STDIN_FILENO, &editor.og_termios) == -1){
        printEditorError("Get terminal attributes upon enabling raw mode error");
    }
    atexit(disableRawInput);

    struct termios raw = editor.og_termios;

    // Disabling all possible flags bitwise
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
    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &editor.og_termios) == -1)
    {
        printEditorError("Set terminal attributes upon disabling raw mode error");
    }
}

void printEditorError(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    // *pain.jpg*
    perror(s);
    exit(EXIT_FAILURE);
}

int getWindowSize(int *rows, int *columns){
    struct winsize windowSize;
    
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == -1 || windowSize.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        }
        return getCursorPos(rows, columns);
    }else
    {
        *rows = windowSize.ws_row;
        *columns = windowSize.ws_col;
        return 0;
    }
}

int getCursorPos(int *rows, int *columns){

    char buf[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1)
    {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, columns) != 2) return -1;

    return 0;
}

// Row operation functions

void editorAppendRow(char *s, size_t len) {

    editor.row = realloc(editor.row, sizeof(editorRow) * (editor.numRows + 1));

    int at = editor.numRows;

    editor.row[at].size = len;
    editor.row[at].chars = malloc(len + 1);
    memcpy(editor.row[at].chars, s, len);
    editor.row[at].chars[len] = '\0';
    editor.numRows++;
}

// Input functions

void moveCursor(int c){
    switch(c){
        case ARROW_UP:
            if(editor.cy != 0)
            {
                editor.cy--;
            }
            break;
        case ARROW_LEFT:
            if(editor.cx != 0)
            {
                editor.cx--;
            }
            break;
        case ARROW_DOWN:
            if(editor.cy < editor.numRows)
            {
                editor.cy++;
            }
            break;
        case ARROW_RIGHT:
            editor.cx++;
            break;
    }
}

void processInputs(){

    int c = readKey();

    switch (c)
    {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);

        case HOME_KEY:
            editor.cx = 0;
            break;
        case END_KEY:
            editor.cx = editor.terminalColumns - 1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = editor.terminalRows;
                while(times--)
                {
                    moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
    }
}

// Appending the buffer

struct appendBuffer
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct appendBuffer *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct appendBuffer *ab){
    free(ab->b);
}

// Output functions

void drawRows(struct appendBuffer *ab){
    for(int i = 0; i < editor.terminalRows; i++)
    {
        int fileRow = i + editor.rowOffset;
        if(fileRow >= editor.numRows)
        {
            if(editor.numRows == 0 && i == editor.terminalRows / 3)
            {
                char welcome[80];
                int welcomeLength = snprintf(welcome, sizeof(welcome),
                "ZText -- Version %s", ZTEXT_VERSION);

                if(welcomeLength > editor.terminalColumns)
                {
                    welcomeLength = editor.terminalColumns;
                }

                int padding = (editor.terminalColumns - welcomeLength) / 2;
                if(padding)
                {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);

                abAppend(ab, welcome, welcomeLength);
            }
            else
            {
                abAppend(ab , "~", 1);
            }
        }else
        {
            int len = editor.row[fileRow].size - editor.columnOffset;
            if (len < 0) len = 0;
            if (len >=editor.terminalColumns) len = editor.terminalColumns;
            abAppend(ab, &editor.row[fileRow].chars[editor.columnOffset], len);
        }

        abAppend(ab, "\x1b[K", 3);
        if(i < editor.terminalRows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void refreshScreen(){
    editorScroll();

    struct appendBuffer ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    drawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (editor.cy - editor.rowOffset) + 1, (editor.cx - editor.columnOffset) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorScroll() {
    if(editor.cy < editor.rowOffset)
    {
        editor.rowOffset = editor.cy;
    }
    if(editor.cy >= editor.rowOffset + editor.terminalRows)
    {
        editor.rowOffset = editor.cy - editor.terminalRows + 1;
    }
    if(editor.cx < editor.columnOffset)
    {
        editor.columnOffset = editor.cx;
    }
    if(editor.cx >= editor.columnOffset + editor.terminalColumns)
    {
        editor.columnOffset = editor.cx - editor.terminalColumns + 1;
    }
}