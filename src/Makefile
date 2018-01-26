#

AR = ar
ARFLAGS = cr
CC = gcc
CFLAGS = -g -O2 -Wall -Werror -Wsign-compare
CPP = gcc -E
CPPFLAGS = 
CXX = g++
CXXCPP = g++ -E

SDL_LIB = -L/usr/local/lib -lSDL2 -lde265 -ljpeg -Wl,-rpath=/usr/local/lib
SDL_INCLUDE = -I/usr/local/include

CXXFLAGS = -g -O2 -std=gnu++11 -Wall -Werror -Wsign-compare -Werror=sign-compare $(SDL_INCLUDE)
LDFLAGS = $(SDL_LIB)

###############################################################

#LIB = libjmheif.a
EXEC = test_heif

#SRCS = $(wildcard *.cc)
#OBJS = $(SRCS: .cc=.o)

OBJS += heif_file.o
OBJS += heif_context.o
OBJS += heif.o
OBJS += error.o
OBJS += box.o
OBJS += libde265_dec_api.o
OBJS += main.o

.PHONY: all

all : $(EXEC)


%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(EXEC) : $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

.PHONY: clean
clean:
	rm $(OBJS) $(LIB) $(EXEC)