CXX=		gcc
CXXFLAGS=	-std=c99 -g -Wall
TARGETS = mandel

all:		$(TARGETS)
	
## MANDEL
mandel: mandel.o bitmap.o
	$(CXX) mandel.o bitmap.o -o mandel -lpthread -lm

mandel.o: mandel.c
	$(CXX) $(CXXFLAGS) -c mandel.c -o mandel.o

bitmap.o: bitmap.c
	$(CXX) $(CXXFLAGS) -c bitmap.c -o bitmap.o

## CLEAN
clean:
	rm -f $(TARGETS) *~ *.o *.bmp
	rm -rf *.dSYM
	