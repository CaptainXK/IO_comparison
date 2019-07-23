test.app:main.o
	gcc $< -o $@ -laio
%.o:%.c
	gcc -c $< 

.PHONY:clean

clean:
	rm -rf *.app *.o *.txt
