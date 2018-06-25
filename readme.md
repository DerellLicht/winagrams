### winagrams - anagram builder

The Windows version of this anagram program was created 
by Derell Licht (herd ill Celt).  

This application is copyright (c) 2010-2016  Daniel D Miller
This program, and its source code, are distributed as freeware.
You can use them for any purpose, personal or commercial, in whole or in part,
for any purpose that you wish, without contacting me further.

winagrams is freeware, source code is available.  
Download [winagrams source code](https://github.com/DerellLicht/winagrams) here

Download [Windows installer](https://github.com/DerellLicht/bin/raw/master/winagrams.setup.exe) here

<hr>
This application is built using the MinGW toolchain.
It also requires certain Cygwin tools (rm, make, etc)

To build command-line version, run
   make anagram

To build the Windows version, run
   make

//****************************************************************  
The original command-line anagram program, in C source code,
was created and distributed by Martin Guy (a Grimy Nut);  
it is available from http://anagram.sourceforge.net/

//****************************************************************
The virtual listview code for the terminal (output) window is derived from
the VListVw application in the Microsoft Platform SDK (2003).

//****************************************************************
The dictionary file (words) was obtained from some web site somewhere,
but unfortunately I cannot currently figure out where that was!!
It is the /usr/share/dict/words file which comes with some fairly recent
version of Linux...  Anyway, stripped out all 24000+ words which contained
apostrophes, but otherwise the file is unmodified.

Later note: the Git repository contains a number of different dictionary files,
obtained from many different sources, mostly unprovenanced. 

//****************************************************************
A note from Derell Licht regarding dictionaries;
I've found over time, that it's better to have a relatively small dictionary,
rather than a huge, comprehensive one.  The problem with huge dictionaries,
is that you end up with, literally, millions - even tens or hundreds of millions -
of results, from relatively trivial search patterns... at least from my experience,
more than about 10 pages of results, end up not being very manageable...



