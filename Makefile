SHELL=cmd.exe
USE_DEBUG = NO

ifeq ($(USE_DEBUG),YES)
CFLAGS=-Wall -O -g
LFLAGS=
else
CFLAGS=-Wall -O3
LFLAGS=-s
endif
CFLAGS += -Wno-write-strings
CFLAGS += -Weffc++

# link library files
CFLAGS += -Ider_libs
CSRC=der_libs/common_funcs.cpp \
der_libs/common_win.cpp \
der_libs/statbar.cpp \
der_libs/wthread.cpp \
der_libs/winmsgs.cpp \
der_libs/cterminal.cpp \
der_libs/vlistview.cpp 

# link application-specific sources
CSRC+=winagrams.cpp anagram.cpp thread.cpp 

OBJS = $(CSRC:.cpp=.o) rc.o

BIN=winagrams.exe

#************************************************************
%.o: %.cpp
	g++ $(CFLAGS) -c $< -o $@

all: $(BIN)

clean:
	rm -f *.exe *.zip *.bak $(OBJS) Output/*

lint:
	cmd /C "c:\lint9\lint-nt +v -width(160,4) -ic:\lint9 -ider_libs mingw.lnt -os(_lint.tmp) lintdefs.cpp $(CSRC)"

depend:
	makedepend $(CFLAGS) $(CSRC)

source:
	zip -D winagrams.zip *
	zip -r winagrams.zip ../der_libs/*

anagram:
	g++ -Wall -O2 -s anagram.cline.cpp -o anagram.exe	

#************************************************************

$(BIN): $(OBJS)
	g++ $(CFLAGS) -mwindows -s $(OBJS) -o $@ -lcomctl32
	\\InnoSetup5\iscc /Q winagrams.iss

rc.o: winagrams.rc 
	windres $< -O coff -o $@

# DO NOT DELETE

der_libs/common_funcs.o: der_libs/common.h
der_libs/common_win.o: der_libs/common.h der_libs/commonw.h
der_libs/statbar.o: der_libs/common.h der_libs/commonw.h der_libs/statbar.h
der_libs/wthread.o: der_libs/wthread.h
der_libs/cterminal.o: der_libs/common.h der_libs/commonw.h
der_libs/cterminal.o: der_libs/cterminal.h der_libs/vlistview.h
der_libs/vlistview.o: der_libs/common.h der_libs/commonw.h
der_libs/vlistview.o: der_libs/vlistview.h
winagrams.o: resource.h der_libs/common.h der_libs/commonw.h winagrams.h
winagrams.o: der_libs/statbar.h der_libs/cterminal.h der_libs/vlistview.h
anagram.o: resource.h der_libs/common.h der_libs/cterminal.h
anagram.o: der_libs/vlistview.h
thread.o: resource.h der_libs/common.h der_libs/commonw.h winagrams.h
thread.o: der_libs/wthread.h
