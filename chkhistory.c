#include <config.h>


/* If we don't use history databases, then these routines will */
/* get compiled and used.  If we do, the routines in chkhistory_db.c will */
/* be used. */

#include <stdio.h>
#include <string.h>

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "chkhistory.h"
#include "suckutils.h"

#ifdef TIMER
#include "timer.h"
#endif

#ifdef CHECK_HISTORY_OLD

/*------------------------------------------------------------*/
void chkhistory(PMaster master) {

	FILE *fhist;
	char linein[MAXLINLEN+1], *ptr;
	int found, nrfound = 0;
	PList curr, prev;

	if((fhist = fopen(HISTORY_FILE, "r")) == NULL) {
		error_log(ERRLOG_REPORT, "Can not open %s, Skipping\n", HISTORY_FILE);
	}
	else {
		fprintf(master->msgs, "Processing History File - Old version ");
		fflush(master->msgs);	/* so msg gets printed */
#ifdef TIMER
		TimerFunc(TIMER_START, 0L);
#endif

		/* read each line and check on our list */
		while(fgets(linein, MAXLINLEN, fhist) != NULL) {
			/* truncate at end of article-nr */
			if((ptr = strchr(linein, '>'))  == NULL) {
				error_log(ERRLOG_REPORT, "Bogus line, ignoring: %s", linein);
			}
			else {
				*(ptr+1) = '\0';

				/* now that we've isolated the article nr, search our list */
				curr = master->head;
				prev = NULL;
				found = FALSE;
				while(found == FALSE && curr != NULL) {
					if(cmp_msgid(curr->msgnr, linein) == TRUE) {
						found = TRUE;
					}
					else {
						prev = curr;
						curr = curr->next;
					}
				}		
				if(found == TRUE) {
#ifdef DEBUG2
					do_debug("Matched %s, nuking\n", curr->msgnr);
#endif
					/* Already on system, remove from list */
					nrfound++;
					master->nritems--;
					if(prev == NULL) {
						/* remove master node */
						master->head = curr->next;
					}
					else {
						prev->next = curr->next;
					}
					free_one_node(curr);
				}
			}			
		}
#ifdef TIMER
		fprintf(master->msgs, "%s\n", TimerFunc(TIMER_TIMEONLY, 0l));
#endif
		fclose(fhist);
		fprintf(master->msgs, "Processed history, %d dupes removed\n", nrfound);
	}
}
/*---------------------------------------------------------------------------------------------------------------*/
#else /* CHECK_HISTORY_OLD */
/*---------------------------------------------------------------------------------------------------------------*/
/* In order to not check the entire linked list every time, let's try something different			 */
/* We'll build an array of pointers to the linked list, based on the first character of the article id		 */
/* then we only have to check the article ids that match the first character, saving searching thru the list	 */
/*---------------------------------------------------------------------------------------------------------------*/
#include <stdlib.h>

#define NR_LETTERS 128		/* nr of chars indexed, only 128 since rfc doesn't support > 128 */

void chkhistory(PMaster master) {
	FILE *fhist;
	char linein[MAXLINLEN+1], *ptr;
	int i, j, x, found, nrfound = 0;
	PList curr, prev, *lets;
	long tlet[NR_LETTERS];
	struct {
		long nr;
		long start;
	} letters[NR_LETTERS];	

	if((fhist = fopen(HISTORY_FILE, "r")) == NULL) {
		error_log(ERRLOG_REPORT, "Can not open %s, Skipping\n", HISTORY_FILE);
	}
	else {
		fprintf(master->msgs, "Processing History File ");
		fflush(master->msgs);	/* so msg gets printed */
#ifdef TIMER
		TimerFunc(TIMER_START, 0L);
#endif
		/* pass one */
		/* count the letters, so we can build the array and also index it */
		
		/* initialize the arrays */
		for(i=0;i<NR_LETTERS;i++) {
			letters[i].nr = letters[i].start = tlet[i] = 0;
		}
		curr = master->head;
		while(curr != NULL) {
			letters[((curr->msgnr[1]) % NR_LETTERS)].nr++;
			/* the % is in case a wacko article id slips by */
			curr = curr->next;
		}
		/* now build the array with the starting points for each */
		/* nr 0 starts on zero so skip it */
		for(i=1;i<NR_LETTERS;i++) {
			letters[i].start = letters[i-1].start + letters[i-1].nr;
		}
	
		/* pass two */
		/* first malloc the array */
		if ((lets = (PList *) calloc(master->nritems, sizeof(PList))) == NULL) {
			error_log(ERRLOG_REPORT, "Out of Memory, skipping history check\n");
		}
		else {
			/* now put them in place */
			curr = master->head;
			while(curr != NULL) {
				j = (curr->msgnr[1]) % NR_LETTERS;
				i = letters[j].start + tlet[j];
				tlet[j]++;	/* so go to next slot */
				lets[i] = curr;
				curr = curr->next;
			}
			/* pass three */
			/* now we can read history file and check against first letter only */
			while(fgets(linein, MAXLINLEN, fhist) != NULL) {
				j = (linein[1]) % NR_LETTERS;
				if((ptr = strchr(linein, '>'))  == NULL) {
					error_log(ERRLOG_REPORT, "Bogus line, ignoring: %s", linein);
				}
				else {
					*(ptr+1) = '\0';
					/* now that we've isolated the article nr, search our list */
					found = FALSE;
					for(i=0;i<letters[j].nr && found == FALSE;i++) {
						x = letters[j].start+i;
						if(lets[x] != NULL && cmp_msgid(linein, lets[x]->msgnr)==TRUE) {
							nrfound++;
							found = TRUE;
							/* now flag it for deletion */
							lets[x]->msgnr[0] = '\0';	/* no more article nr */
							lets[x] = NULL;			/* so don't check it again */
						}
					}
				}
			}
			/* pass four */
			/* now go thru and delete em */
			curr = master->head;
			prev = NULL;
			while(curr != NULL) {			
				if( curr->msgnr[0] == '\0') {
					/* nuke it */
					master->nritems--;
					if(prev == NULL) {
						/* remove master node */
						master->head = curr->next;
						free_one_node(curr);
						curr = master->head;
					}
					else {
						prev->next = curr->next;
						free_one_node(curr);
						curr = prev->next;
					}
				}
				else {
					prev = curr;
					curr = curr->next;
				}
			}
			free(lets);
		}
#ifdef TIMER
		fprintf(master->msgs, "%s\n", TimerFunc(TIMER_TIMEONLY, 0l));
#endif
		fclose(fhist);
		fprintf(master->msgs, "Processed history, %d dupes removed\n", nrfound);
	}
}
#endif /* CHECK_HISTORY_EXP */
