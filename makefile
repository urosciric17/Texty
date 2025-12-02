CC=gcc
LDFLAGS=-lncurses
OUT=texty
IN=curses.c
.PHONY:clean
$(OUT):$(IN)
	@$(CC) $(IN) -o $(OUT) $(LDFLAGS)
	@ls
clean:
	@if [ -f $(OUT) ] ; \
	then \
		rm $(OUT) ; \
		ls ; \
	else \
		printf "Error: The file does not exist..." ; \
	fi
