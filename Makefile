OBJ = ufed-curses.o ufed-curses-checklist.o ufed-curses-help.o

ufed-curses: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) -lcurses

.c.o:
	$(CC) $(CFLAGS) -D_XOPEN_SOURCE=500 -c $<

clean:
	-rm -f ufed-curses *.o

ufed-curses.o: \
	ufed-curses.c ufed-curses.h
ufed-curses-checklist.o: \
	ufed-curses-checklist.c ufed-curses.h ufed-curses-help.h
ufed-curses-help.o: \
	ufed-curses-help.c ufed-curses.h
