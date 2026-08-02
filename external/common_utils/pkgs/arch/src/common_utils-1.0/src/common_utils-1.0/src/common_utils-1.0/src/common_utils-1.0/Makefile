CC=gcc
CFLAGS=-Wall -Wextra -O2 -fPIC -I.

LIB_NAME=libcommon_utils.so

SRC_DIR=src

SRCS = \
$(SRC_DIR)/args.c \
$(SRC_DIR)/elevate.c \
$(SRC_DIR)/file_utils.c \
$(SRC_DIR)/have.c \
$(SRC_DIR)/httpsrv.c \
$(SRC_DIR)/parsing_utils.c \
$(SRC_DIR)/path_utils.c \
$(SRC_DIR)/process_utils.c \
$(SRC_DIR)/strings.c \
$(SRC_DIR)/unit_utils.c \
$(SRC_DIR)/which_os.c

OBJS=$(SRCS:.c=.o)

# ---- default ----
all: $(LIB_NAME)

# ---- shared library ----
$(LIB_NAME): $(OBJS)
	$(CC) -shared -o $@ $^

# ---- object build ----
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- optional standalone HTTP server ----
httpsrv: $(SRC_DIR)/httpsrv.c $(LIB_NAME)
	$(CC) $(CFLAGS) -D_STANDALONE_HTTPSRV \
		$(SRC_DIR)/httpsrv.c \
		-L. -lcommon_utils \
		-o httpsrv

# ---- install (useful for testing outside pacman) ----
install: all
	install -d $(DESTDIR)/usr/include/common_utils
	install -m644 *.h $(DESTDIR)/usr/include/common_utils/
	install -d $(DESTDIR)/usr/lib
	install -m755 $(LIB_NAME) $(DESTDIR)/usr/lib/

	@if [ -f httpsrv ]; then \
		install -Dm755 httpsrv $(DESTDIR)/usr/bin/httpsrv; \
	fi

# ---- clean ----
clean:
	rm -f $(SRC_DIR)/*.o $(LIB_NAME) httpsrv