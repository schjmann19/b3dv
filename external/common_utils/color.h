#ifndef SIMPLE_COLORS_H
#define SIMPLE_COLORS_H

/* ANSI Escape Sequence */
#define ESC "\x1b["

/* Reset */
#define RESET ESC "0m"

/* Text Styles */
#define BOLD          ESC "1m"
#define DIM           ESC "2m"
#define ITALIC        ESC "3m"
#define UNDERLINE     ESC "4m"
#define BLINK         ESC "5m"
#define REVERSE       ESC "7m"
#define HIDDEN        ESC "8m"
#define STRIKETHROUGH ESC "9m"

/* Regular Foreground Colors */
#define FG_BLACK      ESC "30m"
#define FG_RED        ESC "31m"
#define FG_GREEN      ESC "32m"
#define FG_YELLOW     ESC "33m"
#define FG_BLUE       ESC "34m"
#define FG_MAGENTA    ESC "35m"
#define FG_CYAN       ESC "36m"
#define FG_WHITE      ESC "37m"

/* Bright Foreground Colors */
#define FG_BRIGHT_BLACK    ESC "90m"
#define FG_BRIGHT_RED      ESC "91m"
#define FG_BRIGHT_GREEN    ESC "92m"
#define FG_BRIGHT_YELLOW   ESC "93m"
#define FG_BRIGHT_BLUE     ESC "94m"
#define FG_BRIGHT_MAGENTA  ESC "95m"
#define FG_BRIGHT_CYAN     ESC "96m"
#define FG_BRIGHT_WHITE    ESC "97m"

/* Background Colors */
#define BG_BLACK      ESC "40m"
#define BG_RED        ESC "41m"
#define BG_GREEN      ESC "42m"
#define BG_YELLOW     ESC "43m"
#define BG_BLUE       ESC "44m"
#define BG_MAGENTA    ESC "45m"
#define BG_CYAN       ESC "46m"
#define BG_WHITE      ESC "47m"

/* Bright Background Colors */
#define BG_BRIGHT_BLACK    ESC "100m"
#define BG_BRIGHT_RED      ESC "101m"
#define BG_BRIGHT_GREEN    ESC "102m"
#define BG_BRIGHT_YELLOW   ESC "103m"
#define BG_BRIGHT_BLUE     ESC "104m"
#define BG_BRIGHT_MAGENTA  ESC "105m"
#define BG_BRIGHT_CYAN     ESC "106m"
#define BG_BRIGHT_WHITE    ESC "107m"

/* 256-color helpers */
#define FG256(n) "\x1b[38;5;" #n "m"
#define BG256(n) "\x1b[48;5;" #n "m"

/* Cursor Control */
#define CURSOR_HOME    ESC "H"
#define CLEAR_SCREEN   ESC "2J"
#define CLEAR_LINE     ESC "2K"
#define SAVE_CURSOR    ESC "s"
#define RESTORE_CURSOR ESC "u"

/* Convenience */
#define COLOR_RESET RESET

#endif /* SIMPLE_COLORS_H */