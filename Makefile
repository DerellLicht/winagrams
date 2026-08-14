#SHELL=cmd.exe
USE_DEBUG = NO
USE_UNICODE = NO
USE_CLANG = YES

include der_libs\tool_select.mak

ifeq ($(USE_DEBUG),YES)
CFLAGS=-Wall -O -g -c
LFLAGS= -mwindows
else
CFLAGS=-Wall -O3 -c
LFLAGS=-s -mwindows
endif
CFLAGS += -Wno-write-strings
CFLAGS += -Weffc++
CFLAGS += -Wno-c++11-narrowing

# link library files
CFLAGS += -Ider_libs
CSRC=der_libs/common_funcs.cpp \
der_libs/common_win.cpp \
der_libs/hyperlinks.cpp \
der_libs/statbar.cpp \
der_libs/wthread.cpp \
der_libs/winmsgs.cpp \
der_libs/cterminal.cpp \
der_libs/vlistview.cpp 

# link application-specific sources
CSRC+=winagrams.cpp anagram.cpp thread.cpp about.cpp

OBJS = $(CSRC:.cpp=.o) rc.o

BIN=winagrams
BINS=$(BIN).exe

LIBS = -lcomdlg32 -lgdi32

#************************************************************
%.o: %.cpp
	$(TOOLS)/$(GNAME) $(CFLAGS) $< -o $@

all: $(BIN).exe

clean:
	rm -f *.exe *.zip *.bak $(OBJS)

dist:
	rm -f *.zip
	zip $(BIN).zip $(BINS) readme.md dict

ctidy_all:
	cmd /C "clang-tidy $(CSRC) -- $(CFLAGS) 2>&1 | grep -oP '\[\K[a-z][a-z0-9-]+(?=\]$$)' | sort | uniq -c | sort -rn"

ctidy_local:
	cmd /C "clang-tidy $(CAPPSRC) -- $(CFLAGS) 2>&1 | grep -oP '\[\K[a-z][a-z0-9-]+(?=\]$$)' | sort | uniq -c | sort -rn"

ctidy_libs:
	cmd /C "clang-tidy $(CLIBSRC) -- $(CFLAGS) 2>&1 | grep -oP '\[\K[a-z][a-z0-9-]+(?=\]$$)' | sort | uniq -c | sort -rn"

clint:
	cmd /C "python ..\ClaudeLint.py --exclude der_libs"
	
cppc:
	cmd /C "cppcheck --project=compile_commands.json --std=c++14 --suppressions-list=./.suppress.cppcheck"

check:
	cmd /C "d:\llvm\bin\clang-tidy.exe $(CSRC) -- $(CFLAGS) "

depend:
	makedepend $(CFLAGS) $(CSRC)

anagram:
	g++ -Wall -O2 -s anagram.cline.cpp -o anagram.exe	

#************************************************************

$(BINS): $(OBJS)
	$(TOOLS)/$(GNAME) $(OBJS) $(LFLAGS) -o $(BINS) $(LIBS) 

rc.o: winagrams.rc 
	$(TOOLS)\$(WRNAME) $< -O COFF -o $@

# DO NOT DELETE

der_libs/common_funcs.o: der_libs/common.h
der_libs/common_win.o: der_libs/common.h der_libs/commonw.h
der_libs/hyperlinks.o: der_libs/iface_32_64.h der_libs/hyperlinks.h
der_libs/statbar.o: der_libs/common.h der_libs/commonw.h der_libs/statbar.h
der_libs/wthread.o: der_libs/wthread.h
der_libs/cterminal.o: der_libs/common.h der_libs/commonw.h
der_libs/cterminal.o: der_libs/cterminal.h der_libs/vlistview.h
der_libs/vlistview.o: der_libs/common.h der_libs/commonw.h
der_libs/vlistview.o: der_libs/vlistview.h
winagrams.o: version.h resource.h der_libs/common.h der_libs/commonw.h
winagrams.o: winagrams.h der_libs/statbar.h der_libs/cterminal.h
winagrams.o: der_libs/vlistview.h der_libs/winmsgs.h
anagram.o: resource.h der_libs/common.h winagrams.h der_libs/cterminal.h
anagram.o: der_libs/vlistview.h
thread.o: resource.h der_libs/common.h der_libs/commonw.h winagrams.h
thread.o: der_libs/wthread.h
about.o: resource.h version.h der_libs/hyperlinks.h
