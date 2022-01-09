# Makefile

PROG = fsv
SRCS = fsv.c info.c proc.c util.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -static


.PHONY: all clean

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

all: $(PROG)

$(OBJS): extern.h

clean:
	@rm -f $(PROG) *.o
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
