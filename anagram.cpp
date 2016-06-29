// #ifndef lint
// static char sccsid[] = "@(#)anagram.c 1.16 20/6/89";
// #endif

//****************************************************************************
//  Original C source, as of 01/01/10 10:16
//  http://anagram.sourceforge.net/
//****************************************************************************
//  12/31/09 04:40  Dan Miller
//     > Updated to build under gcc
//     > convert to cpp
//     > Switch from using redirection to read the dictionary, 
//       to passing as command-line argument.
//       This is preparation for doing a Windows version of the program.
//  
//**********************************************************************************
/*
 *	Find all anagrams of a word or phrase which can be made from the words
 *	of the dictionary which should come in on stdin. All words containing
 *	characters not in the key are rejected out of hand so punctuated and
 *	capitalised words are usually lost.
 *	Eliminates all one-letter "words" except a, i, o.
 *
 *	BUG due to optimisation N: 
 *	If an anagram contains a word twice, there will be duplicate phrases
 *	put out. EG peter collinson
 *		respect ill no no
 *		respect ill no on ***
 *		respect ill on no ***
 *		respect ill on on
 *	This is because the printing routine regurgitates all combinations of
 *	the words, so we get "on no" and "no on". Can be fixed by marking
 *	active words in the printing routine, but it is fiddly.
 *
 *	Martin Guy, December 1985
 */

/*
 * Optimisation N-3:
 * Sort the wordlist into longest-first. This affects the behaviour of the
 * recursive calls and takes 1/2 the time against a random ordering,
 * 1/6th of the time against shortest-first.
 *
 * Optimisation N-2
 * As each pass works through the wordlist seeing what words will fit the
 * letters that are left over from the key once the parent's words have been
 * extracted, it stores this subset of the complete list and passes it down
 * as the wordlist which its child is to work from. A word which is not
 * useful to us is sure not to be useful to any of our children.
 *
 * Optimisation N-1
 * Instead of processing the list from the top down, process it from the bottom
 * up. This makes it easier to build the sub-lists for children, but requires a
 * doubly-linked list to be able to trace back up it. This means that the
 * wordlist has to be sorted the other way for optimisation N-2
 *
 * Optimisation N
 * The words "mart" and "tram" show the same behaviour when computing the
 * anagrams. We can reduce the work we have to do by shortening the wordlist by
 * storing only one of each such word in the wordlist and having the "synonyms"
 * dangling from its cell on a linked list. They are all churned out again in
 * the printing stage.
 * Result:		old	new
 * stuart plant		10.7	9.6
 * peter collinson	297.9	169.7
 * peter collinson -m3	101.5	64.8
 *
 * Optimisation N+1 (unimplemented)
 * For short anagrams, the lion's share of the time is spent in fgets().
 * It is silly to read into the stdio buffer, copy across into local buffer
 * and process from there when we could use getc(), save a fn call per word
 * and reduce the amount of shuffling being done.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>				  //  PATH_MAX

#include <windows.h>

#include "resource.h"
#include "common.h"
#include "cterminal.h"

//  winagrams.cpp
extern CTerminal *myTerminal ;
extern void status_message(char *msgstr);
extern void clear_message_area(void);
extern void update_listview(void);

#define MAXWORDS 64        /* Maximum number of words in output phrases */
									/* Also determines maximum recursion depth */
/*
 *	Structure of a cell used to hold a word in the list which has the same
 *	letters as a word we have already found. Idem is latin for "the same".
 */
typedef struct idem
{
	char *word;						  /* the word exactly as read from the dict */
	struct idem *link;			  /* next in the list of similar words */
} idem_t;

/*
 *	Structure of each word read from the dictionary and stored as part
 *	of a possible anagram.
 */
typedef struct cell
{
	struct cell *link;			  /* To bind the linked list together */
	char *word;						  /* At last! The word itself */
	int wordlen;					  /* length of the word */

	/* Sub-wordlist reduces problem for children. These pointers are
	 * the heads of a stack of doubly linked lists (!) */
	struct cell **flink;			  /* Forward links for doubly linked list */
	struct cell **rlink;			  /* Reverse links for doubly linked list */

	/* First element in linked list of words which contain the same letters
	 * (including the original) exactly as they came out of the dict */
	idem_t idem;
} cell_t;

static int freq[256];                    /* number of time each character occurs in the key.
										     * Must be initialised to 0s.
										     * Set by buildwordlist, Used by findanags. */
static int nletters;                  /* Number of letters in key, == sum(freq[*]) */
static cell_t *anagword[MAXWORDS];    /* pointers to the cells for the words
										     * making up the anagram under construction */
static int nwords;				  /* number of words in current list */

static int const vflag = 0;                    /* verbose mode: give a running commentary */
static int const iflag = 0;                    /* Ignore case of words' first letters */
static int const iiflag = 0;                /* decapitalise all characters from dict */
static int const pflag = 0;                    /* Ignore punctuation in words from dict */
static int const purify = 0;                /* Some munging has to be done on the dict's words */
			/* purify == (iflag || pflag || iiflag) */
static int maxgen = 0;                /* Highest number of generations of findanag possible */

static uint solutions_found = 0 ;

uint min_word_len = 3;

static char word_list_name[PATH_MAX + 1] = "words";
static cell_t *wordlist = NULL ;

//  function prototypes
static cell_t *buildwordlist (void);
static int sameletters (char *word1, char *word2);
static int islonger (char *first, char *second);
static cell_t *sort (cell_t * head);
static void initfind (cell_t * head);
static void findanags (int generation, cell_t * wordlt, int nleft);
static cell_t *forgelinks (cell_t * head);
static void print (int gen, int higen);
static char *savestr (char *oldstr);

//************************************************************************
//  add functions for linked mode
//************************************************************************

//  actually, this just creates the fully-qualified path/filename
//  for the dictionary file.
bool read_word_list(char *wordlist_filename)
{
   if (wordlist_filename != NULL) {
      strncpy (word_list_name, wordlist_filename, PATH_MAX);
      word_list_name[PATH_MAX] = 0;
   } else {
      if (derive_file_path(word_list_name, "words") != 0) {
         syslog("Cannot derive default dictionary filename\n");
         return false;
      }
   }
//    wordlist = buildwordlist ();
//    if (wordlist == NULL) {
//       syslog("Empty dictionary or no suitable words.\n");
//       return false;
//    }
// 
//    wordlist = sort (wordlist);
   return true ;
}

#define  MAX_PKT_CHARS   80

//********************************************************************
//  this is a memory leak, but I'm getting confused
//  trying to figure out what to delete and where.
//********************************************************************
static void delete_wordlist(void)
{
   if (wordlist != NULL) {
      // cell_t *wordptr = wordlist ;
      wordlist = NULL ;
      // while (wordptr != NULL) {
      //    cell_t *killptr = wordptr ;
      //    wordptr = wordptr->link ;
      //    free(killptr->idem.word) ;
      //    free(killptr->idem.link) ;
      //    free(killptr->word) ;
      //    free(killptr) ;
      // }
   }

   // uint idx ;
   // for (idx=0; idx<MAXWORDS; idx++) {
   //    anagword[idx] = NULL ;
   // }
   
   maxgen = 0 ;
}

//********************************************************************
void find_anagrams(HWND hwnd)
{
   char msgstr[81] ;
   //  read data out of hwnd:IDC_PHRASE
   char input_bfr[MAX_PKT_CHARS+1] ;
   uint input_bfr_len = GetWindowTextA (GetDlgItem(hwnd,IDC_PHRASE) , input_bfr, MAX_PKT_CHARS);
   if (input_bfr_len > MAX_PKT_CHARS) 
       input_bfr_len = MAX_PKT_CHARS ;
   input_bfr[MAX_PKT_CHARS] = 0 ;
   if (vflag)  //lint !e506 !e774
      syslog("find_anagrams: [%u] [%s]\n", input_bfr_len, input_bfr) ;

   status_message("Begin new anagram search") ;

   // clear_message_area(&this_term);
   clear_message_area();
   delete_wordlist() ;

   ZeroMemory((char *) &freq[0], sizeof(freq)) ;
   nletters = 0 ;
   for (char *p = input_bfr; *p != 0; p++) {
      if (*p != ' ') {
         freq[(uint) (u8) *p]++ ;
         nletters++;
      }
   }
   if (maxgen == 0)
       maxgen = nletters;
   
   wordlist = buildwordlist ();
   if (wordlist == NULL) {
      syslog("Empty dictionary or no suitable words.\n");
      return ;
   }

   wordlist = sort (wordlist);

   initfind (wordlist);

   solutions_found = 0 ;
   findanags (0, forgelinks (wordlist), nletters);
   if (solutions_found == 0) {
      status_message("no anagrams found for input string !") ;
   } else {
      // reverse_list_entries() ;
      // InsertListViewItems(solutions_found);  //  This triggers drawing of listview
      myTerminal->reverse_list_entries() ;
      update_listview();

      wsprintf(msgstr, "%u anagrams found", solutions_found) ;
      status_message(msgstr) ;
   }
}

char *get_dict_filename(void)
{
   return word_list_name;
}

/*
 *	Read words in from stdin and put candidates into a linked list.
 *	Return the head of the list.
 * Note that this function does *not* merely read the word list into memory!1
 * It checks the words against the source phrase.
 * So this cannot be called at program startup, without a source phrase
 */
static cell_t *buildwordlist (void)
{
	register char *cp;			  /* Temporary for word-grubbing */
	char *word;						  /* Candidate word from dictionary */
	char realword[32];			  /* Exactly as read from dictionary */
	cell_t *head = NULL;			  /* Head of the linked list */
	cell_t **lastptr = &head;	  /* Points to the last link field in the chain */
	cell_t *newcell;				  /* Pointer to cell under construction */
	cell_t *cellp;					  /* list follower for idem spotting */
	int len;							  /* store length of candidate word */
   if (vflag)  //lint !e506 !e774
      syslog("reading [%s]\n", word_list_name);
	FILE *fd = fopen (word_list_name, "rt");
	if (fd == NULL) {
      syslog("%s: %s", word_list_name, strerror(errno)) ;
		return NULL;
	}

	nwords = 0;						  /* number of words in wordlist */
	// while (fgets(realword, sizeof(realword), stdin) != NULL) {
   uint read_lines = 0 ;
	while (fgets (realword, sizeof (realword), fd) != NULL) {
      read_lines++ ;
		/*
		 * Zap the newline
		 */
      // for (cp = realword; *cp != '\0'; cp++) {
      //   if (*cp == '\n') {
      //      *cp = '\0';
      //      break;
      //   }
      // }
      strip_newlines(realword) ;
      if (strlen (realword) < min_word_len) {
			continue;
      }
		/*
		 * if iflag or pflag, filter the word before processing it.
		 */
      if (!purify) {  //lint !e506 !e774
			word = realword;
		}
		else {
			static char buf[32];
			register char *rp, *wp;
			register int differ = 0;
			for (rp = realword, wp = buf; *rp != '\0'; rp++) {
            if (*rp >= 'a'  &&  *rp <= 'z') {
               *wp++ = *rp;
            } else 
            if (*rp >= '0'  &&  *rp <= '9') {
               *wp++ = *rp;
            } else
            if (*rp >= 'A'  &&  *rp <= 'Z') {
               if (iiflag || (iflag && rp == realword)) {  //lint !e506 !e774
                  *wp++ = (char) tolower(*rp);
                  differ = 1;
               }
               else {
                  *wp++ = *rp;
               }
            } else {
               if (pflag) {  //lint !e506 !e774
                  differ = 1;
               }
               else {
                  *wp++ = *rp;
               }
            }
         }
			*wp = '\0';
			word = differ ? buf : realword;
		}

		/*
		 * Throw out all one-letter words except for a, i, o.
		 */
		if (word[1] == '\0') {
			switch (word[0]) {
				case 'a':
				case 'i':
				case 'o':
					break;
				default:
					goto nextword;
			}
		}

		/*
		 * Reject the word if it contains any character which
		 * wasn't in the key.
		 */
		for (cp = word; *cp != '\0'; cp++) {
         if (freq[(uint) (u8) *cp] == 0) {
				/* The word contains an illegal char */
				goto nextword;
				/* shame about the break statement */
			}
		}
		/*
		 * This word merits further inspection. See if it contains
		 * no more of any letter than the original.
		 */
		for (cp = word; *cp != '\0'; cp++) {
         if (freq[(uint) (u8) *cp] <= 0) {
				/* We have run out of this letter.
				 * try next word */
				goto restore;
			}							  /* else */
         freq[(uint) (u8) *cp]--;
		}

		len = cp - word;			  /* the length of the word */
		/*
		 * See if this word contains the same letters as a previous one
		 * If so, tack it on to that word's idem list.
		 */
		/* Scan down the wordlist looking for a match */
		for (cellp = head; cellp != NULL; cellp = cellp->link) {
			if (len == cellp->wordlen && sameletters (word, cellp->word)) {
				/* Seen one like this before.
				 * Put it on the idem list */
				register idem_t *newidem;
				newidem = (idem_t *) malloc (sizeof (idem_t));
            if (newidem == NULL) {
               syslog("build word list: out of memory\n");
               return 0;
            }
				newidem->word = savestr (realword);
				newidem->link = cellp->idem.link;
				cellp->idem.link = newidem;
				goto restore;
			}
		}

		/*
		 * the word passed all the tests
		 * Construct a new cell and attach it to the list.
		 */
		/* Get a new cell */
		newcell = (cell_t *) malloc (sizeof (cell_t));
      if (newcell == NULL) {
         syslog("build word list: out of memory\n");
         return 0;
      }
      newcell->word = savestr (word);
		/* remember the length for optimising findanag and sorting */
		newcell->wordlen = cp - word;
		/* If realword differs from pure word, store it separately */
//  lint: anagram.cpp  501  Info 774: Boolean within 'if' always evaluates to True 
//          [Reference: file anagram.cpp: lines 392, 423, 501]
      if (realword == word) { //lint !e774
			newcell->idem.word = newcell->word;
		}
		else {
			newcell->idem.word = savestr (realword);
		}
		/* Only one item in idem list so far; mark end of list */
		newcell->idem.link = NULL;
		/* Mark it as the end of the list */
		newcell->link = NULL;
		/* Point the old tail-cell at it */
		*lastptr = newcell;
		/* And remember the new one as the tail cell */
		lastptr = &(newcell->link);
		nwords++;
	 restore:
		/*
		 * Restore freq[] to its original state; this is quicker than
		 * doing a block copy into a temporary array and munging that.
		 * At this point, cp is either pointing to the terminating null
		 * or to the first letter in the word for which freq == 0.
		 * So all the freq[] entries for [word..(cp-1)] have been
		 * decremented. Put them back.
		 */
		for (--cp; cp >= word; --cp) {
         freq[(uint) (u8) *cp]++;
		}
	 nextword:;
	}
   if (vflag)  //lint !e506 !e774
      syslog("%d [%u] suitable words found\n", nwords, read_lines);
   fclose (fd);   
	return (head);
}

/*
 *	Do the two words contain the same letters?
 *	It must be guaranteed by the caller that they are the same length.
 */
static int sameletters (char *word1, char *word2)
{
   static int slfreq[256];
   static bool init_done = false ;
   if (!init_done) {
      ZeroMemory(slfreq, sizeof(slfreq)) ;
      init_done = true ;
   }
   
   register char *cp;           /* to skip through words */
	for (cp = word1; *cp != '\0'; cp++)
      slfreq[(uint) (u8) *cp]++;
	for (cp = word2; *cp != '\0'; cp++) {
      if (--slfreq[(uint) (u8) *cp] < 0) {
			/* fail! undo the damage done by word2 */
			while (cp >= word2)
            slfreq[(uint) (u8) *cp--]++;
			/* and by word1 */
         // Warning 445: Reuse of for loop variable 'cp' could cause chaos
         for (cp = word1; *cp != '\0'; cp++) //lint !e445
            slfreq[(uint) (u8) *cp]--;
			return (0);
		}
	}
	/* success! slfreq must be all 0s now. */
	return (1);
}

/*
 *	Comparison function for sort so that longest words come first
 */
static int islonger (char *first, char *second)
{
   //  I can't really tell if these lint warnings are valid or not...
// anagram.cpp  566  Info 826: Suspicious pointer-to-pointer conversion (area too small)
// anagram.cpp  566  Info 826: Suspicious pointer-to-pointer conversion (area too small)
   return ((*(cell_t **) second)->wordlen - (*(cell_t **) first)->wordlen);   //lint !e826
}

/*
 *	Sort the wordlist by word length so that the longest is at the head.
 *	Return the new head of the list.
 */
static cell_t *sort (cell_t * head)
{
	register cell_t **sortbase;  /* Array of pointers to cells */
	register int i;				  /* Loop index */
	register cell_t *cp;			  /* Loop index */
	cell_t *newhead;				  /* hold return value so we can free */
	sortbase = (cell_t **) malloc ((unsigned) nwords * sizeof (cell_t *));
   if (sortbase == NULL) {
      syslog("sort: out of memory\n") ;
      return NULL;
   }
	/*
	 * Reference all strings from the array
	 */
	for (cp = head, i = 0; cp != NULL; cp = cp->link, i++) {
		sortbase[i] = cp;
	}

	/* Check just in case */
	if (i != nwords) {
		/* nwords disagrees with the length of the linked list */
      syslog("Aagh! Despair! %d %d\n", i, nwords);
		abort ();
	}

	// qsort((char *) sortbase, nwords, sizeof(cell_t *), islonger);
	qsort ((char *) sortbase, nwords, sizeof (cell_t *),
		(int (*)(const void *, const void *)) islonger);
	/*
	 * Re-forge the linked list according to the sorted array of pointers
	 */
	for (i = 0; i < nwords - 1; i++) {
		sortbase[i]->link = sortbase[i + 1];
	}
	sortbase[nwords - 1]->link = NULL;
	newhead = sortbase[0];
	free ((char *) sortbase);
	return (newhead);
}

static void initfind (cell_t * head)
{
	cell_t *cp;
	/*
	 * Get some memory for the sub-wordlists
	 */
	for (cp = head; cp != NULL; cp = cp->link) {
		cp->flink = (cell_t **) malloc ((unsigned) maxgen * sizeof (cell_t *));
      if (cp->flink == NULL) {
         syslog("initfind: out of memory\n") ;
         return ;
      }
		cp->rlink = (cell_t **) malloc ((unsigned) maxgen * sizeof (cell_t *));
      if (cp->rlink == NULL) {
         syslog("initfind: out of memory\n") ;
         return ;
      }
	}
}

/*
 *	Print out all anagrams which can be made from the wordlist wordl
 *	out of the letters left in freq[].
 *	(cell)->[fr]link[generation] is the word list we are to scan.
 *	Scan from the tail back to the head; (head->rlink[gen]==NULL)
 */
static void findanags (int generation, cell_t * wordlt, int nleft)
// int generation;     /* depth of recursion */
// cell_t *wordlt;     /* the tail of the wordlist we are to use */
// int nleft;    /* number of unclaimed chars from key */
{
	register cell_t *cellp;		  /* for tracing down the wordlist */
	register char *cp;			  /* for inspecting each word */
	register int nl;				  /* copy of nleft for munging */
	register char *cp2;			  /* temp for speed */
	register cell_t *myhead = NULL;	/* list of words we found suitable */
	register cell_t *mytail = NULL;	/* tail of our list of words */

	/*
	 * Loop from the tail cell back up to and including the
	 * head of the wordlist
	 */
	for (cellp = wordlt; cellp != NULL; cellp = cellp->rlink[generation]) {
		/*
		 * This looks remarkably like bits of buildwordlist.
		 *
		 * First a quick rudimentary check whether we have already
		 * run out of any of the letters required.
		 */
		for (cp = cellp->word; *cp != '\0'; cp++) {
         if (freq[(uint) (u8) *cp] == 0)
				goto nextword;
		}

		/*
		 * Now do a more careful counting check.
		 */
		nl = nleft;
		for (cp = cellp->word; *cp != '\0'; cp++) {
         if (freq[(uint) (u8) *cp] == 0)
				goto failure;
         freq[(uint) (u8) *cp]--;
			nl--;
		}

		/*
		 * Yep, there were the letters left to make the word.
		 * Are we done yet?
		 */
		switch (nl) {
			case 0:					  /* Bingo */
				/* insert the final word */
				anagword[generation] = cellp;
				/* and print the phrase */
				print (0, generation);
            break;
			default:
				if (generation < maxgen - 1) {
					/* Record the word and find something to follow it */
					/*
					 * Add it to the list of words that were ok for
					 * us; those words which we rejected are
					 * certainly not worth our children's attention.
					 * Constructed like a lifo stack.
					 */
					cellp->flink[generation + 1] = myhead;
					if (myhead != NULL)
						myhead->rlink[generation + 1] = cellp;
					else				  /* this is the first item on the list */
						mytail = cellp;
					myhead = cellp;
					myhead->rlink[generation + 1] = NULL;
					/* record where we are for printing */
					anagword[generation] = cellp;
					/* and try all combinations of words
					 * on this stem */
					findanags (generation + 1, mytail, nl);
				}
		}

	 failure:						  /*
										     *  Restore freq to its former state
										   */
		cp2 = cellp->word;
		for (--cp; cp >= cp2; cp--) {
         freq[(uint) (u8) *cp]++;
		}
	 nextword:;
	}
}

/*
 *	Procedure used to print out successful anagrams.
 *	Because of Optimisation #n, we have to churn out every combination
 *	of the words in anagword[0..gen]. Best done recursively.
 *
 *	Print anagram phrases indicated by anagword[0..gen].
 *	There are <gen> invocations of this procedure active above us.
 *	The words they are contemplating are available through
 *	idlist[0..gen-1]. Print the parents' words from there followed by
 *	every combination of the words dangling from anagword[gen..maxgen].
 */
static void print(int gen, int higen)
{
	static char *idlist[MAXWORDS];	/* list of our parents' words */
	if (gen == higen) {
		/* No further recursion; just print. */
		register idem_t *ip;		  /* follow down anagword[higen] */
		register int i;			  /* index along idlist */
		/* for each word in idemlist[gen], print the stem and it */
		for (ip = &(anagword[higen]->idem); ip != NULL; ip = ip->link) {
         char bfr[MAXWORDS+1] ;
         int slen = 0 ;
         bfr[0] = 0 ;
			/* there must always be at least ONE word. */
			for (i = 0; i < higen; i++) {
            slen += wsprintf(&bfr[slen], "%s ", idlist[i]) ;   //lint !e727  idlist not init
			}
         // puts (ip->word);
         // syslog("e%s ", ip->word) ;
         slen += wsprintf(&bfr[slen], "%s ", ip->word) ;

         //  change this to stuff into a list, 
         //  build listview when done
         myTerminal->put(bfr) ;
         // update_listview();   //  this causes the flickering

         solutions_found++ ;
      }
	}
	else {
		/* recurse */
		register idem_t *ip;
		for (ip = &(anagword[gen]->idem); ip != NULL; ip = ip->link) {
			idlist[gen] = ip->word;
			print (gen + 1, higen);
		}
	}
}

/*
 *	Forge the reverse links to form a doubly-linked list
 *	Return the address of the tail of the list.
 */
static cell_t *forgelinks (cell_t * head)
{
	cell_t *prev = NULL;			  /* address of previous cell */
	cell_t *cp;						  /* list follower */
	for (cp = head; cp != NULL; cp = cp->link) {
		cp->flink[0] = cp->link;
		cp->rlink[0] = prev;
		prev = cp;
	}
	return (prev);
}

/*
 *	Save a copy of a string in some mallocked memory
 *	and pass back a pointer to it. 
 */
static char *savestr (char *oldstr)
{
	register char *cp;			  /* roving pointer for character-grubbing */
	register char *np;			  /* roving pointer into new string */
	/* find end of string for length */
   // for (cp = oldstr; *cp != '\0'; cp++);
   uint old_len = strlen(oldstr) ;
   char *newstr = (char *) malloc (old_len + 1);   /* +1 for the null */
   if (newstr == NULL) {
      syslog("savestr: out of memory\n") ;
      return NULL;
   }
   for (cp = oldstr, np = newstr; (*np++ = *cp++) != '\0';) /* copy */
      ;
	return (newstr);
}

