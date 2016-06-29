#ifndef lint
static char sccsid[] = "@(#)anagram.c	1.16 20/6/89";
#endif

/*
 *	Find all anagrams of a word or phrase which an be made from the words
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
#include <ctype.h>

#define MAXWORDS 64		/* Maximum number of words in output phrases */
				/* Also determines maximum recursion depth */

/*
 *	Structure of a cell used to hold a word in the list which has the same
 *	letters as a word we have already found. Idem is latin for "the same".
 */
typedef struct idem {
	char *word;		/* the word exactly as read from the dict */
	struct idem *link;	/* next in the list of similar words */
} idem_t;

/*
 *	Structure of each word read from the dictionary and stored as part
 *	of a possible anagram.
 */

typedef struct cell {
	struct cell *link;	/* To bind the linked list together */
	char *word;		/* At last! The word itself */
	int wordlen;		/* length of the word */

	/* Sub-wordlist reduces problem for children. These pointers are
	 * the heads of a stack of doubly linked lists (!) */
	struct cell **flink;	/* Forward links for doubly linked list */
	struct cell **rlink;	/* Reverse links for doubly linked list */

	/* First element in linked list of words which contain the same letters
	 * (including the original) exactly as they came out of the dict */
	idem_t	idem;
} cell_t;

int freq[256];		/* number of time each character occurs in the key.
			 * Must be initialised to 0s.
			 * Set by buildwordlist, Used by findanags. */
int nletters;		/* Number of letters in key, == sum(freq[*]) */
cell_t *anagword[MAXWORDS];	/* pointers to the cells for the words
				 * making up the anagram under construction */
static int nwords;	/* number of words in current list */

cell_t *buildwordlist();
cell_t *sort();
cell_t *forgelinks();
char *savestr();
char *malloc();
char *strcpy();

char *progname;		/* program name for error reporting */
int vflag = 0;		/* verbose mode: give a running commentary */
int iflag = 0;		/* Ignore case of words' first letters */
int iiflag = 0;		/* decapitalise all characters from dict */
int pflag = 0;		/* Ignore punctuation in words from dict */
int purify = 0;		/* Some munging has to be done on the dict's words */
			/* purify == (iflag || pflag || iiflag) */
int maxgen = 0;		/* Highest number of generations of findanag possible */

main(argc, argv)
char **argv;
{
	register int i, j;
	cell_t *wordlist;

	progname = argv[0];
	
	/*
	 *	Check calling syntax
	 */
	for (i=1; i<argc && argv[i][0] == '-'; i++) {
	    for (j=1; argv[i][j] != '\0'; j++) {
		switch (argv[i][j]) {
		case 'I':	/* Ignore case completely */
			iiflag = 1;
			purify = 1;
			break;
		case 'i':	/* Initial letter case independent mode */
			iflag = 1;
			purify = 1;
			break;
		case 'p':	/* punctuation-ignoring mode */
			pflag = 1;
			purify = 1;
			break;
		case 'v':	/* verbose mode */
			vflag = 1;
			break;
		case 'm':	/* Set maximum number of words in anagram */
			maxgen = atoi(&argv[i][j+1]);
			if (maxgen <= 0) goto usage;
			goto nextarg;	/* gobbles rest of arg */
			break;
		default:
			goto usage;
		}
	    }
nextarg:    ;
	}

	if (i >= argc) {
usage:		fprintf(stderr, "Usage: %s -v -m# word [word] ...\n", progname);
		fprintf(stderr, "-v\tVerbose: give running commentary.\n");
		fprintf(stderr, "-m#\tMaximum number of words in anagrams.\n");
		fprintf(stderr, "-i\tIgnore case of initial letters of words from dictionary.\n");
		fprintf(stderr, "-I\tIgnore case of all letters of words from dictionary.\n");
		fprintf(stderr, "-p\tIgnore punctuation in words from dictionary.\n");
		exit(1);
	}

	/* i must remain intact until word args have been scanned. */

	if (isatty(0)) {
		if (freopen("/usr/dict/words", "r", stdin) == NULL) {
			fprintf(stderr, "%s: cannot open dictionary.\n", progname);
			exit(1);
		}
	}

	/*
	 *	Fill in frequency of letters in argument.
	 *	freq[*] must have been initialised to 0.
	 */
	nletters = 0;
	for ( /* i left by flag loop */ ; i<argc; i++) {
		for (j=0; argv[i][j] != '\0'; j++) {
			freq[argv[i][j]]++;
			nletters++;
		}
	}

	if (vflag) fprintf(stderr, "%d letters in key.\n", nletters);

	/*
	 *	If no -m option, set the maximum possible recursion depth
	 */
	if (maxgen == 0) maxgen = nletters;

	/*
	 *	If the machine is not fully loaded this will make no difference.
	 *	If it IS, what are you running this cpu gobbler for?
	 */
	(void) nice(5);

	if (vflag) fputs("Building wordlist...\n", stderr);
	wordlist = buildwordlist();
	if (wordlist == NULL) {
		if (vflag) fputs("Empty dictionary or no suitable words.\n", stderr);
		exit(0);
	}

	if (vflag) fputs("Sorting wordlist...\n", stderr);
	wordlist = sort(wordlist);

	if (vflag) {
		fputs("After sorting:\n", stderr);
		printwordlist(wordlist);
	}

	if (vflag) fputs("Searching for anagrams...\n", stderr);
	initfind(wordlist);
	findanags(0, forgelinks(wordlist), nletters);
	exit(0);
}

/*
 *	Read words in from stdin and put candidates into a linked list.
 *	Return the head of the list.
 */
cell_t *
buildwordlist()
{
	register char *cp;		/* Temporary for word-grubbing */
	char *word;			/* Candidate word from dictionary */
	char realword[32];		/* Exactly as read from dictionary */
	cell_t *head = NULL;		/* Head of the linked list */
	cell_t **lastptr = &head;	/* Points to the last link field in the chain */
	cell_t *newcell;		/* Pointer to cell under construction */
	cell_t *cellp;			/* list follower for idem spotting */
	int	len;			/* store length of candidate word */

	nwords = 0;			/* number of words in wordlist */
	while (fgets(realword, sizeof(realword), stdin) != NULL) {
		/*
		 * Zap the newline
		 */
		for (cp=realword; *cp != '\0'; cp++) {
			if (*cp == '\n') {
				*cp = '\0';
				break;
			}
		}

		/*
		 * if iflag or pflag, filter the word before processing it.
		 */
		if (!purify) {
			word = realword;
		} else {
			static char buf[32];
			register char *rp, *wp;
			register int differ = 0;

			for (rp = realword, wp = buf; *rp != '\0'; rp++) {
				switch (*rp) {
				case 'a': case 'b': case 'c': case 'd':
				case 'e': case 'f': case 'g': case 'h':
				case 'i': case 'j': case 'k': case 'l':
				case 'm': case 'n': case 'o': case 'p':
				case 'q': case 'r': case 's': case 't':
				case 'u': case 'v': case 'w': case 'x':
				case 'y': case 'z':
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9':
					*wp++ = *rp;
					break;

				case 'A': case 'B': case 'C': case 'D':
				case 'E': case 'F': case 'G': case 'H':
				case 'I': case 'J': case 'K': case 'L':
				case 'M': case 'N': case 'O': case 'P':
				case 'Q': case 'R': case 'S': case 'T':
				case 'U': case 'V': case 'W': case 'X':
				case 'Y': case 'Z':
					if (iiflag || (iflag && rp == realword) ) {
						*wp++ = tolower(*rp);
						differ = 1;
					} else {
						*wp++ = *rp;
					}
					break;
				
				default:
					if (pflag) {
						differ = 1;
					} else {
						*wp++ = *rp;
					}
				}
			}
			*wp = '\0';
			word = differ ? buf : realword;
		}

		/*
		 *	Throw out all one-letter words except for a, i, o.
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
		 *	Reject the word if it contains any character which
		 *	wasn't in the key.
		 */
		for (cp = word; *cp != '\0'; cp++) {
			if (freq[*cp] == 0) {
				/* The word contains an illegal char */
				goto nextword;
				/* shame about the break statement */
			}
		}
		/*
		 *	This word merits further inspection. See if it contains
		 *	no more of any letter than the original.
		 */
		for (cp=word; *cp != '\0'; cp++) {
			if (freq[*cp] <= 0) {
				/* We have run out of this letter.
				 * try next word */
				goto restore;
			} /* else */
			freq[*cp]--;
		}

		len = cp - word;	/* the length of the word */
		/*
		 * See if this word contains the same letters as a previous one
		 * If so, tack it on to that word's idem list.
		 */
		/* Scan down the wordlist looking for a match */
		for (cellp = head; cellp != NULL; cellp = cellp->link) {
			if (len == cellp->wordlen && sameletters(word, cellp->word)) {
				/* Seen one like this before.
				 * Put it on the idem list */
				register idem_t *newidem;

				newidem = (idem_t *) malloc(sizeof(idem_t));
				if (newidem == NULL) oom("idemword");
				newidem->word = savestr(realword);
				newidem->link = cellp->idem.link;
				cellp->idem.link = newidem;

				goto restore;
			}
		}

		/*
		 *	the word passed all the tests
		 *	Construct a new cell and attach it to the list.
		 */
		/* Get a new cell */
		newcell = (cell_t *) malloc(sizeof(cell_t));
		if (newcell == NULL) oom("buildwordlist");

		newcell->word = savestr(word);

		/* remember the length for optimising findanag and sorting */
		newcell->wordlen = cp - word;

		/* If realword differs from pure word, store it separately */
		if (realword == word) {
			newcell->idem.word = newcell->word;
		} else {
			newcell->idem.word = savestr(realword);
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
		for ( --cp; cp >= word; --cp ) {
			freq[*cp]++;
		}
nextword:	;
	}
	if (vflag) fprintf(stderr, "%d suitable words found\n", nwords);

	return(head);
}

/*
 *	Do the two words contain the same letters?
 *	It must be guaranteed by the caller that they are the same length.
 */
int
sameletters(word1, word2)
char *word1, *word2;
{
	static int slfreq[256];	/* auto-initialised to 0s. */
	register char *cp;	/* to skip through words */

	for (cp = word1; *cp != '\0'; cp++) slfreq[*cp]++;
	for (cp = word2; *cp != '\0'; cp++) {
		if (--slfreq[*cp] < 0) {
			/* fail! undo the damage done by word2 */
			while (cp >= word2) slfreq[*cp--]++;
			/* and by word1 */
			for (cp = word1; *cp != '\0'; cp++) slfreq[*cp]--;
			return(0);
		}
	}
	/* success! slfreq must be all 0s now. */
	return(1);
}

/*
 *	Comparison function for sort so that longest words come first
 */
static int
islonger(first, second)
char *first, *second;
{
	return((*(cell_t **)second)->wordlen - (*(cell_t **)first)->wordlen);
}

/*
 *	Sort the wordlist by word length so that the longest is at the head.
 *	Return the new head of the list.
 */
cell_t *
sort(head)
cell_t *head;
{
	register cell_t **sortbase;	/* Array of pointers to cells */
	register int i;			/* Loop index */
	register cell_t *cp;		/* Loop index */
	cell_t *newhead;		/* hold return value so we can free */

	sortbase = (cell_t **) malloc((unsigned)nwords * sizeof(cell_t *));
	if (sortbase == NULL) oom("buildwordlist while sorting");

	/*
	 *	Reference all strings from the array
	 */
	for (cp=head,i=0; cp!=NULL; cp=cp->link,i++) {
		sortbase[i] = cp;
	}

	/* Check just in case */
	if (i != nwords) {
		/* nwords disagrees with the length of the linked list */
		fprintf(stderr, "%s: Aagh! Despair! %d %d\n",
			progname, i, nwords);
		abort();
	}

	qsort((char *) sortbase, nwords, sizeof(cell_t *), islonger);

	/*
	 * Re-forge the linked list according to the sorted array of pointers
	 */
	for (i=0; i<nwords-1; i++) {
		sortbase[i]->link = sortbase[i+1];
	}
	sortbase[nwords-1]->link = NULL;

	newhead = sortbase[0];
	free((char *)sortbase);
	return(newhead);
}

/* Out Of Memory */
oom(where)
char *where;
{
	fprintf(stderr, "%s: Out of memory in %s.\n", progname, where);
	exit(1);
}

initfind(head)
cell_t *head;
{
	cell_t *cp;

	/*
	 *	Get some memory for the sub-wordlists
	 */
	for (cp = head; cp != NULL; cp = cp->link) {
		cp->flink = (cell_t **) malloc((unsigned)maxgen * sizeof(cell_t *));
		if (cp->flink == NULL) oom("initfind");
		cp->rlink = (cell_t **) malloc((unsigned)maxgen * sizeof(cell_t *));
		if (cp->rlink == NULL) oom("initfind");
	}
}

/*
 *	Print out all anagrams which can be made from the wordlist wordl
 *	out of the letters left in freq[].
 *	(cell)->[fr]link[generation] is the word list we are to scan.
 *	Scan from the tail back to the head; (head->rlink[gen]==NULL)
 */
findanags(generation, wordlt, nleft)
int generation;		/* depth of recursion */
cell_t *wordlt;		/* the tail of the wordlist we are to use */
int nleft;		/* number of unclaimed chars from key */
{
	register cell_t *cellp;		/* for tracing down the wordlist */
	register char *cp;		/* for inspecting each word */
	register int nl;		/* copy of nleft for munging */
	register char *cp2;		/* temp for speed */
	register cell_t *myhead = NULL;	/* list of words we found suitable */
	register cell_t *mytail;	/* tail of our list of words */

	/*
	 *	Loop from the tail cell back up to and including the
	 *	head of the wordlist
	 */
	for (cellp = wordlt; cellp != NULL; cellp = cellp->rlink[generation]) {
		/*
		 *	This looks remarkably like bits of buildwordlist.
		 *
		 *	First a quick rudimentary check whether we have already
		 *	run out of any of the letters required.
		 */
		for (cp = cellp->word; *cp != '\0'; cp++) {
			if (freq[*cp] == 0) goto nextword;
		}

		/*
		 *	Now do a more careful counting check.
		 */
		nl = nleft;
		for (cp = cellp->word; *cp != '\0'; cp++) {
			if (freq[*cp] == 0) goto failure;
			freq[*cp]--; nl--;
		}

		/*
		 *	Yep, there were the letters left to make the word.
		 *	Are we done yet?
		 */
		switch (nl) {
		case 0:	/* Bingo */
			/* insert the final word */
			anagword[generation] = cellp;
			/* and print the phrase */
			print(0, generation);
			break;
		default:
			if (generation < maxgen-1) {
				/* Record the word and find something to follow it */
				/*
				 * Add it to the list of words that were ok for
				 * us; those words which we rejected are
				 * certainly not worth our children's attention.
				 * Constructed like a lifo stack.
				 */
				cellp->flink[generation+1] = myhead;
				if (myhead != NULL)
					myhead->rlink[generation+1] = cellp;
				else /* this is the first item on the list */
					mytail = cellp;
				myhead = cellp;
				myhead->rlink[generation+1] = NULL;

				/* record where we are for printing */
				anagword[generation] = cellp;
				/* and try all combinations of words
				 * on this stem */
				findanags(generation+1, mytail, nl);
			}
		}

failure:	/*
		 *	Restore freq to its former state
		 */
		cp2 = cellp->word;
		for (--cp; cp>=cp2; cp--) {
			freq[*cp]++;
		}
nextword:	;
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
print(gen, higen)
{
	static char *idlist[MAXWORDS];	/* list of our parents' words */

	if (gen == higen) {
		/* No further recursion; just print. */
		register idem_t *ip;	/* follow down anagword[higen] */
		register int i;		/* index along idlist */

		/* for each word in idemlist[gen], print the stem and it */
		for (ip = &(anagword[higen]->idem); ip != NULL; ip = ip -> link) {
			/* there must always be at least ONE word. */
			for (i=0; i<higen; i++) {
				fputs(idlist[i], stdout);
				putchar(' ');
			}
			puts(ip->word);
		}
	} else {
		/* recurse */
		register idem_t *ip;

		for (ip = &(anagword[gen]->idem); ip != NULL; ip = ip->link) {
			idlist[gen] = ip->word;
			print(gen+1, higen);
		}
	}

	(void) fflush(stdout);
}

/*
 *	Forge the reverse links to form a doubly-linked list
 *	Return the address of the tail of the list.
 */
cell_t *
forgelinks(head)
cell_t *head;
{
	cell_t *prev = NULL;	/* address of previous cell */
	cell_t *cp;		/* list follower */

	for (cp = head; cp != NULL; cp = cp->link) {
		cp -> flink[0] = cp -> link;
		cp -> rlink[0] = prev;
		prev = cp;
	}
	return(prev);
}

/*
 *	Save a copy of a string in some mallocked memory
 *	and pass back a pointer to it. 
 */
char *
savestr(oldstr)
char *oldstr;
{
	register char *cp;	/* roving pointer for character-grubbing */
	register char *np;	/* roving pointer into new string */
	char *newstr;		/* new copy of string to be passed back */

	/* find end of string for length */
	for (cp=oldstr; *cp != '\0'; cp++);

	newstr = malloc((unsigned) (cp-oldstr+1));	/* +1 for the null */
	if (newstr == NULL) oom("savestr");
	for (cp=oldstr, np=newstr; (*np++ = *cp++) != '\0'; );	/* copy */
	return(newstr);
}

printwordlist(head)
cell_t *head;
{
	cell_t *cp;	/* list tracer */
	idem_t *ip;	/* idem list tracer */
	int col = 0;	/* print column for word wrapping */
	int wordlen;	/* length of realword */

	for (cp = head; cp != NULL; cp = cp -> link) {
		for (ip = &(cp->idem); ip != NULL; ip = ip->link) {
			wordlen = strlen(ip->word);
			if (col + wordlen + 1 > 79) {
				putc('\n', stderr);
				col = 0;
			}
			if (col != 0) {
				putc(' ', stderr);
				col++;
			}
			fputs(cp->word, stderr);
			col += wordlen;
		}
	}
	putc('\n', stderr);
}
