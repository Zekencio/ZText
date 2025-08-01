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
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdbool.h>

// CURRENT STEP (TO DO): Step 131 (Beginning of chapter 6)

// Constants and macros

#define ZTEXT_VERSION "0.1"
#define ZTEXT_TAB_STOP 4
#define ZTEXT_QUIT_TIMES 2
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey{
    BACKSPACE = 127,
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
    int renderSize;
    char *chars;
    char *render;
} editorRow;

struct config
{
    int cx, cy;
    int rx;
    int rowOffset;
    int columnOffset;
    struct termios og_termios;
    int terminalRows;
    int terminalColumns;
    int numRows;
    char *fileName;
    char statusMsg[80];
    time_t statusMsg_time;
    editorRow *row;
    bool stinky;
};

struct config editor;

struct appendBuffer
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

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
void editorInsertRow(int at, char *s, size_t len);
void editorScroll();
void editorUpdateRow(editorRow *row);
int editorRowCxToRx(editorRow *row, int cx);
void drawStatusBar(struct appendBuffer *ab);
void setStatusMessage(const char *fmt, ...);
void editorRowInsertChar(editorRow *row, int at, int c);
void editorInsertChar(int c);
char* editorRowsToString(int* bufLength);
void editorFreeRow(editorRow *row);
void editorDelRow(int at);
char* editorPrompt(char *prompt);

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
    editor.rx = 0;
    editor.rowOffset = 0;
    editor.columnOffset = 0;
    editor.numRows = 0;
    editor.row = NULL;
    editor.fileName = NULL;
    editor.statusMsg[0] = '\0';
    editor.statusMsg_time = 0;
    editor.stinky = false;

    if(getWindowSize(&editor.terminalRows, &editor.terminalColumns) == -1){
        printEditorError("Window size error");
    }

    editor.terminalRows -= 2;
    setStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");
}

// File input/output functions

void editorOpen(char* fileName){
    free(editor.fileName);
    editor.fileName = strdup(fileName);

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
        editorInsertRow(editor.numRows, line, lineLength);
    }
    free(line);
    fclose(fp);
    editor.stinky = false;
}

char* editorRowsToString(int* bufLength) {
    int totalLength = 0;
    int i;
    for (i = 0; i < editor.numRows; i++) {
        totalLength += editor.row[i].size + 1;
    }
    *bufLength = totalLength;

    char *buffer = malloc(totalLength);
    char *p = buffer;
    for (i = 0; i < editor.numRows; i++) {
        memcpy(p, editor.row[i].chars, editor.row[i].size);
        p += editor.row[i].size;
        *p = '\n';
        p++;
    }

    return buffer;
}

void editorSaveFile() {
    if (editor.fileName == NULL)
    {
        editor.fileName = editorPrompt("Save file as: %s (Press ESC to cancel)");
        if (editor.fileName == NULL)
        {
            setStatusMessage("Saving sequence aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowsToString(&len);
    int fd = open(editor.fileName, O_RDWR | O_CREAT, 0644);
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                editor.stinky = false;
                setStatusMessage("Saving successful. %d bytes written on disk.", len);
                return;
            }
        }
        close(fd);
    }

    free(buf);
    setStatusMessage("File saving aborted. I/O Error: %s", strerror(errno));
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

int editorRowCxToRx(editorRow *row, int cx){
    int rx = 0;

    for (int i = 0; i < cx; i++) {
        if(row->chars[i] == '\t')
        {
            rx += (ZTEXT_TAB_STOP - 1) - (rx % ZTEXT_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

void editorUpdateRow(editorRow *row) {
    int tabs = 0;
    int i;

    for (i = 0; i < row->size; i++) if (row->chars[i] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(ZTEXT_TAB_STOP - 1) + 1);

    int index = 0;
    for (i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t')
        {
            row->render[index++] = ' ';
            while (index % ZTEXT_TAB_STOP != 0) row->render[index++] = ' ';
        }else {
            row->render[index++] = row->chars[i];
        }
    }

    row->render[index] = '\0';
    row->renderSize = index;
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > editor.numRows) return;

    editor.row = realloc(editor.row, sizeof(editorRow) * (editor.numRows + 1));
    memmove(&editor.row[at + 1], &editor.row[at], sizeof(editorRow) * (editor.numRows - at));

    editor.row[at].size = len;
    editor.row[at].chars = malloc(len + 1);
    memcpy(editor.row[at].chars, s, len);
    editor.row[at].chars[len] = '\0';

    editor.row[at].renderSize = 0;
    editor.row[at].render = NULL;
    editorUpdateRow(&editor.row[at]);

    editor.numRows++;
    if (!editor.stinky) {
        editor.stinky = true;
    }
}

void editorRowInsertChar(editorRow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    if (!editor.stinky) {
        editor.stinky = true;
    }
}

void editorFreeRow(editorRow *row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if (at < 0 || at >= editor.numRows) return;
    editorFreeRow(&editor.row[at]);
    memmove(&editor.row[at], &editor.row[at] + 1,
        sizeof(editorRow) * (editor.numRows - at - 1));
    editor.numRows--;
    if (!editor.stinky) {
        editor.stinky = true;
    }
}

void editorRowDelChar(editorRow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    if (!editor.stinky) {
        editor.stinky = true;
    }
}

void editorRowAppendString(editorRow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    if (!editor.stinky) {
        editor.stinky = true;
    }
}

// Editor operations

void editorInsertChar(int c){
    if (editor.cy == editor.numRows) editorInsertRow(editor.numRows,"", 0);
    editorRowInsertChar(&editor.row[editor.cy], editor.cx, c);
    editor.cx++;
}

void editorInsertNewLine() {
    if (editor.cx == 0)
    {
        editorInsertRow(editor.cy, "", 0);
    }else
    {
        editorRow *row = &editor.row[editor.cy];
        editorInsertRow(editor.cy + 1, &row->chars[editor.cx], row->size - editor.cx);
        row = &editor.row[editor.cy];
        row->size = editor.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    editor.cy++;
    editor.cx = 0;
}

void editorDelChar() {
    if (editor.cy == editor.numRows) return;
    if (editor.cx == 0 && editor.cy == 0) return;

    editorRow *row = &editor.row[editor.cy];
    if (editor.cx > 0)
    {
        editorRowDelChar(row, editor.cx - 1);
        editor.cx--;
    }else
    {
        editor.cx = editor.row[editor.cy - 1].size;
        editorRowAppendString(&editor.row[editor.cy - 1], row->chars, row->size);
        editorDelRow(editor.cy);
        editor.cy--;
    }
}

// Input functions

char* editorPrompt(char *prompt) {
    size_t bufSize = 128;
    char *buf = malloc(bufSize);

    size_t bufLength = 0;
    buf[0] = '\0';

    while (true)
    {
        setStatusMessage(prompt, buf);
        refreshScreen();

        int c = readKey();

        if (c == DELETE_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if (bufLength != 0) buf[--bufLength] = '\0';
        }else if (c == '\x1b')
        {
            setStatusMessage("");
            free(buf);
            return NULL;
        }else if (c == '\r')
        {
            if (bufLength != 0)
            {
                setStatusMessage("");
                return buf;
            }
        }else if (!iscntrl(c) && c < 128)
        {
            if (bufLength == bufSize - 1)
            {
                bufSize *= 2;
                buf = realloc(buf, bufSize);
            }
            buf[bufLength++] = c;
            buf[bufLength] = '\0';
        }
    }
}

void moveCursor(int c){

    editorRow *row = editor.cy >= editor.numRows ? NULL : &editor.row[editor.cy];

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
            }else if(editor.cy > 0)
            {
                editor.cy--;
                editor.cx = editor.row[editor.cy].size;
            }
            break;
        case ARROW_DOWN:
            if(editor.cy < editor.numRows)
            {
                editor.cy++;
            }
            break;
        case ARROW_RIGHT:
            if(row && editor.cx < row->size)
            {
                editor.cx++;
            }else if (row && editor.cx == row->size)
            {
                editor.cy++;
                editor.cx = 0;
            }
            break;
    }

    row = (editor.cy >= editor.numRows) ? NULL : &editor.row[editor.cy];
    int rowLength = row ? row->size : 0;
    if (editor.cx > rowLength) {
        editor.cx = rowLength;
    }
}

void processInputs(){
    static int quit_times = ZTEXT_QUIT_TIMES;

    int c = readKey();

    switch (c)
    {
        case '\r':
            editorInsertNewLine();
            break;
        case CTRL_KEY('q'):
            if (editor.stinky && quit_times > 0) {
                setStatusMessage("WARNING! File has unsaved changes."
                    "Press Ctrl-Q %d more time(s) to quit", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
        case CTRL_KEY('s'):
            editorSaveFile();
            break;
        case HOME_KEY:
            editor.cx = 0;
            break;
        case END_KEY:
            if (editor.cy < editor.numRows)
                editor.cx = editor.row[editor.cy].size;
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DELETE_KEY:
            if (c == DELETE_KEY) moveCursor(ARROW_RIGHT);
            editorDelChar();
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c == PAGE_UP)
                {
                    editor.cy = editor.rowOffset;
                }else
                {
                    editor.cy = editor.rowOffset + editor.terminalRows - 1;
                    if (editor.cy > editor.numRows) editor.cy = editor.numRows;
                }

                int times = editor.terminalRows;
                while(times--)
                {
                    moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        default:
            editorInsertChar(c);
            break;
    }

    quit_times = ZTEXT_QUIT_TIMES;
}

// Appending the buffer

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
            int len = editor.row[fileRow].renderSize - editor.columnOffset;
            if (len < 0) len = 0;
            if (len >= editor.terminalColumns) len = editor.terminalColumns;
            abAppend(ab, &editor.row[fileRow].render[editor.columnOffset], len);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void drawStatusBar(struct appendBuffer *ab) {
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
      editor.fileName ? editor.fileName : "[No Name]", editor.numRows,
      editor.stinky ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
      editor.cy + 1, editor.numRows);
    if (len > editor.terminalColumns) len = editor.terminalColumns;
    abAppend(ab, status, len);
    while (len < editor.terminalColumns) {
        if (editor.terminalColumns - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void drawMessageBar(struct appendBuffer *ab) {
    abAppend(ab, "\x1b[K", 3);
    int messageLength = strlen(editor.statusMsg);
    if(messageLength > editor.terminalColumns)
        messageLength = editor.terminalColumns;
    if (messageLength && time(NULL) - editor.statusMsg_time < 5)
        abAppend(ab, editor.statusMsg, messageLength);
}

void refreshScreen(){
    editorScroll();

    struct appendBuffer ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    drawRows(&ab);
    drawStatusBar(&ab);
    drawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",  (editor.cy - editor.rowOffset) + 1,
                                                            (editor.rx - editor.columnOffset) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorScroll() {
    editor.rx = 0;
    if(editor.cy < editor.numRows)
    {
        editor.rx = editorRowCxToRx(&editor.row[editor.cy], editor.cx);
    }

    if(editor.cy < editor.rowOffset)
    {
        editor.rowOffset = editor.cy;
    }
    if(editor.cy >= editor.rowOffset + editor.terminalRows)
    {
        editor.rowOffset = editor.cy - editor.terminalRows + 1;
    }
    if(editor.rx < editor.columnOffset)
    {
        editor.columnOffset = editor.rx;
    }
    if(editor.rx >= editor.columnOffset + editor.terminalColumns)
    {
        editor.columnOffset = editor.rx - editor.terminalColumns + 1;
    }
}

void setStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(editor.statusMsg, sizeof(editor.statusMsg), fmt, ap);
    va_end(ap);
    editor.statusMsg_time = time(NULL);
}
