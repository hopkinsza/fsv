CFLAGS += -Wall

.PHONY: clean

fsv: fsv.c

clean:
	@rm fsv
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
