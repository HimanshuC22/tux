/*** INCLUDES ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define TUX_VERSION "1.0.0"

/*** DATA ***/
struct editorConfiguration
{
	int screen_rows;
	int screen_cols;
	struct termios orig_termios;
};

struct editorConfiguration config;

/*** TERMINAL ***/
void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

void disableRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.orig_termios) == -1)
		die("tcsetattr");
}

void enableRawMode()
{
	if(tcgetattr(STDIN_FILENO, &config.orig_termios) == -1) die("tcgetattr"); // get the terminal attribures in orig_termios

	// atexit will be called at the exit of the program
	// we need to disable raw mode at the exit
	atexit(disableRawMode);

	struct termios raw = config.orig_termios;

	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // modifying input flags to read Ctrl+S and Ctrl+Q

	raw.c_cflag |= (CS8);

	raw.c_oflag &= ~(OPOST); // modifying output flags to disable output processing

	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // modifying local flags of the struct

	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 20;

	// first, echoing is disabled, and then cannonical mode is turned off

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetatr"); // set the modified terminal attribures to the terminal
}

char readKey()
{
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if(nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

int getWindowSize(int * rows, int * cols)
{
	struct winsize windowSize;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) || windowSize.ws_col == 0)
		return -1;
	else
	{
		*rows = windowSize.ws_row;
		*cols = windowSize.ws_col;
		return 0;
	}
}

/*** APPEND BUFFER ***/
struct abuf
{
	char * b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abufAppend(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);
	memcpy(&new[ab->len], s, len);
	if(new == NULL) return;
	ab -> b = new;
	ab -> len += len;
}

void abufFree(struct abuf *ab)
{
	free(ab->b);
}

/*** INPUT ***/
void processKeypress()
{
	char c = readKey();
	switch (c)
	{
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
		        write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** OUTPUT ***/
void drawRows(struct abuf *ab)
{
        int y;
	write(STDOUT_FILENO, &config.screen_rows, 1);
        for(y = 0; y<config.screen_rows; y++)
	{
		if(y == config.screen_rows/3)
		{
			char welcome[80];
			int welcomeLength = snprintf(welcome, sizeof(welcome),
					"TUX EDITOR -- VERSION");
			if(welcomeLength < config.screen_cols) 
			{
				welcomeLength = config.screen_cols;
				abufAppend(ab, welcome, welcomeLength);
			}
		}
		else
		{
			abufAppend(ab, "~", 1);
		}
		abufAppend(ab, "\x1b[K", 3);
		if(y < config.screen_rows - 1)
			abufAppend(ab, "\r\n", 2);
	}
}

void refreshScreen()
{
	struct abuf ab = ABUF_INIT;
	abufAppend(&ab, "\x1b[?25l", 6);
//	abufAppend(&ab, "\x1b[2J", 4);
	abufAppend(&ab, "\x1b[H", 3);
	drawRows(&ab);
	abufAppend(&ab, "\x1b[H", 3);
	abufAppend(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	abufFree(&ab);
}

/*** INIT ***/

void initEditor()
{
	if(getWindowSize(&config.screen_rows, &config.screen_cols) == -1)
		die("getWindowSize");
}

int main()
{
	enableRawMode();
	initEditor();
	refreshScreen();

	while (1)
	{
		processKeypress();
	}

	return 0;
}
