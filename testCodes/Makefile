OBJS = fvvadd.o vvadd.o gcd.o sum.o selectionSort.o insertSort.o addFloat.o mulFloat.o

all: $(OBJS)

%.o: %.c
	clang -O3 -emit-llvm -c -c $< 

clean: 
	rm -rf $(OBJS)

rebuild: clean all