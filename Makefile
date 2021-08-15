# Makefile

PROG = fsv
SRCS = fsv.c info.c proc.c util.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall


.PHONY: all clean

$(PROG):
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(PROG): $(OBJS)

$(OBJS): extern.h

clean:
	@rm -f $(PROG) *.o
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
