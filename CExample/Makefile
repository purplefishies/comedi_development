CFILES		:= $(wildcard *.c)
BINARIES	:= $(CFILES:.c=)


test:
	@echo $(BINARIES)

clean:
	$(RM) -f *.tap
	$(RM) -f $(BINARIES)

%:%.c
	g++ -g  $< -o $@ -lcomedi -lgtest -lpthread	

# all:
# 	g++ -g  pc104_idio16_test.c -o pc104_idio16_test -lcomedi -lgtest -lpthread
