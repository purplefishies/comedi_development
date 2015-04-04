CFILES		:= $(wildcard *.c)
BINARIES	:= $(CFILES:.c=)



clean:
	$(RM) -f *.tap
	$(RM) -f $(BINARIES)

%:%.c
	g++ -g  $< -o $@ -lcomedi -lgtest -lpthread	

test:test.c
	g++ -g  $< -o $@ 
# all:
# 	g++ -g  pc104_idio16_test.c -o pc104_idio16_test -lcomedi -lgtest -lpthread
