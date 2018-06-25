### winagrams - anagram builder

The Windows version of this anagram program was created 
by Derell Licht (herd ill Celt).  

This application is copyright (c) 2010-2016  Daniel D Miller  
This program, and its source code, are distributed as unrestricted freeware.
You can use them for any purpose, personal or commercial, in whole or in part,
for any purpose that you wish, without contacting me further.

Obtain [source code](https://github.com/DerellLicht/winagrams) here

Download [Windows installer](https://github.com/DerellLicht/bin/raw/master/winagrams.setup.exe) here

#### history of this program  
The original command-line anagram program, in C source code,
was created and distributed by Martin Guy (a Grimy Nut); 
it is available from [here](http://anagram.sourceforge.net/)

I wrapped the WinAPI interface around Martin's original code, to make it more accessible to modern users.

#### library files
The default dictionary file (words) was obtained from a relatively recent version of Linux;  
It is the /usr/share/dict/words file.  I stripped out all 24000+ words which contained
apostrophes, but otherwise the file is unmodified.

Later note: the Git repository for winagrams contains a number of different dictionary files,
obtained from many different sources, mostly unprovenanced. 

A note from Derell Licht regarding dictionaries;
I've found over time, that it's better to have a relatively small dictionary,
rather than a huge, comprehensive one.  The problem with huge dictionaries,
is that you end up with, literally, millions - even tens or hundreds of millions -
of results, from relatively trivial search patterns... at least from my experience,
more than about 10 pages of results, end up not being very manageable...

#### building the application
This application is built using the MinGW toolchain; I recommend the [TDM](http://tdm-gcc.tdragon.net/), to avoid certain issues with library accessibility. The makefile also requires certain Cygwin tools (rm, make, etc).
Building the Windows installer will require [Inno Setup](http://jrsoftware.org/isinfo.php); 
I use version 5.37, but later versions should work fine.

To build command-line version, run  
   `make anagram`

To build the Windows version, run  
   `make`



