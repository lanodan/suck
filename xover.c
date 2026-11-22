#include <config.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "suck.h"
#include "suck_config.h"
#include "both.h"
#include "xover.h"
#include "phrases.h"
#include "killfile.h"
#include "suckutils.h"

/*func prototypes */
int do_one_line(PMaster, char *, char *, PGroup);
int chk_a_group(PMaster, POneKill, POverview, char **);
int match_group(char *, char *, int);

/* these MUST match xover_reasons[] ! EXCEPT FOR THE last entry which MUST BE REASON_NONE */
enum { REASON_HIBYTES, REASON_LOWBYTES, REASON_HILINES, REASON_LOWLINES, REASON_HEADER, REASON_NOKEEP, REASON_NONE};

/*----------------------------------------------------------------------*/
int do_group_xover(PMaster master, char *grpname, long startnr) {

	int done, len, nr, retval = RETVAL_OK;
	char *resp, cmd[MAXLINLEN];
	PKillStruct xoverkill;
	PGroup grpkill = NULL;

	sprintf(cmd, "xover %ld-\r\n", startnr);
	retval = send_command(master, cmd, &resp, 0);
	if(retval == RETVAL_OK) {
		number(resp, &nr);
		switch(nr) {
		  default:
		  case 412:
		  case 502:
			  /* abort on these errors */
			  error_log(ERRLOG_REPORT, xover_phrases[0], resp, NULL);
			  retval = RETVAL_ERROR;
			  break;
		  case 420:
			  /* no articles available, no prob */
			  break;
		  case 224:
			  /* first, check to see if there is a group kill file for this group */
			  /* we do this here, so we only do it once per group, since we don't */
			  /* have all the crossposted groups to deal with */
			  xoverkill = master->xoverp;
			  if(xoverkill->totgrps > 0) {
				  for(nr = 0; xoverkill != NULL && nr < xoverkill->totgrps; nr++) {
					  if(match_group(xoverkill->grps[nr].group, grpname, master->debug) == TRUE) {
/* have to set up to use wildcards in group name */
						  grpkill = &(xoverkill->grps[nr]);
						  if(master->debug == TRUE) {
							  do_debug("Using Group Xover killfile\n");
						  }
					  }
				  }
			  }
			  
			  /* okay, do it */
			  done = FALSE;
			  while((done == FALSE) && (retval == RETVAL_OK)) {
				  len = sgetline(master->sockfd, &resp);
				  /* have we got the last one? */
				  if((len == 2) && (strcmp(resp, ".\n") == 0)) {
					  done = TRUE;
				  }
				  else if(len < 0) {
					  retval =RETVAL_ERROR;
				  }
				  else {
					  retval = do_one_line(master, grpname, resp, grpkill);
				  }
			  }
			  break;
		}
	}
	return retval;
}
/*-----------------------------------------------------------------------------------*/
int do_one_line(PMaster master, char *group, char *linein, PGroup grpkill) {

	/* take in one line of xover, then run it thru the kill routines */
	
	int len, msgidlen = 0, retval = RETVAL_OK, match = REASON_NONE;
	char *msgid = NULL, *ptr, *reason = NULL, tmp = '\0';
	long msgnr;
	POverview overv;
	
	PKillStruct masterk = master->xoverp; /* our master killstruct */
	overv = masterk->xoverview;
	ptr  = get_long(linein, &msgnr);  /* get our message number */
	
	/* first go thru and match up the header with the fields in the line, and fill */
	/* in the pointers in the xoverview with pointers to start of field */
	while( *ptr != '\0' && overv != NULL) {
		/* we are just past a tab */
		len = 0;
		overv->field = ptr;
		while(*ptr != '\t' && *ptr != '\0') {
			len++;
			ptr++;
		}
		overv->fieldlen = len;
		
		/* save Messag-ID location for later passing to subroutines */
		if(strcasecmp("Message-ID:", overv->header) == 0) {
			msgid = overv->field;
			msgidlen = overv->fieldlen;
		}
		overv = overv->next;
		if(*ptr == '\t') {
			ptr++; /* advance past tab */
		}
	}
	while(overv != NULL) {
		/* in case we got a short xoverview */
		overv->field = NULL;
		overv->fieldlen = 0;
		overv = overv->next;
	}
	if(msgid == NULL) {
		error_log(ERRLOG_REPORT, xover_phrases[13], linein, NULL);
	}
	else {
		/* we have to check against master killfile and group kill file */
		match = chk_a_group( master, &(masterk->master), masterk->xoverview, &reason);
		if((grpkill != NULL) && (match == REASON_NONE || masterk->grp_override == TRUE)) {
			/* we have to check against group */
		
			match = chk_a_group( master, &(grpkill->match), masterk->xoverview, &reason);
			if(grpkill->delkeep == DELKEEP_KEEP) {
				/* we keep only those that match */
				if(match == REASON_NONE) {
				/* we didn't match, so DON'T keep */
					match = REASON_NOKEEP;
				}
				else {
				/* so we keep it */
					match = REASON_NONE;
				}
			}
		}

		/* now we need to null terminate the msgid, for allocing or printing */

		if(msgid != NULL) {
			tmp = msgid[msgidlen];
			msgid[msgidlen] = '\0';
		}
	
		if(match == REASON_NONE) {
			/* we keep it */
			retval= allocnode(master, msgid, MANDATORY_OPTIONAL, group, msgnr);
		}
		else if(masterk->logyn != KILL_LOG_NONE) {
			/* only open this once */
			if(masterk->logfp == NULL) {
				if((masterk->logfp = fopen(full_path(FP_GET, FP_TMPDIR, master->kill_log_name), "a")) == NULL) {
					MyPerror(xover_phrases[11]);
				}
			}
			
			if(masterk->logfp != NULL) {
				/* Log it */
				if(match == REASON_HEADER) {
					print_phrases(masterk->logfp, xover_phrases[9], group, reason, msgid, NULL);
				}
				else {
					print_phrases(masterk->logfp, xover_phrases[9], group, xover_reasons[match], msgid, NULL);
				}
				
				if(masterk->logyn == KILL_LOG_LONG) {
				/* print the xover as well */
				/* first restore the message-id end of string, so entire line prints */
					if(msgid != NULL) {
						msgid[msgidlen] = tmp;
					}
					print_phrases(masterk->logfp, xover_phrases[10],linein, NULL);
				}
			}
		}
	}
	
	return retval;
}
/*----------------------------------------------------------------------------------------*/
void get_xoverview(PMaster master) {
	/* get in the xoverview.fmt list, so we can parse what xover returns later */
	/* we'll put em in a linked list */
	
	int done, retval, len;
	char *resp;
	POverview tmp, tmp2, curptr;
	PKillStruct killp;
	
	retval = RETVAL_OK;
	killp = master->xoverp;
	curptr = NULL;  /* where we are currently at in the linked list */
	
	if(send_command(master, "list overview.fmt\r\n", &resp, 215) == RETVAL_OK) {
		done = FALSE;
		/* now get em in, until we hit .\n which signifies end of response */
		while(done != TRUE) {
			sgetline(master->sockfd, &resp);
			if(strcmp(resp, ".\n") == 0) {
				done = TRUE;
			}
			else if(retval == RETVAL_OK) {
				/* now save em if we haven't run out of memory */
				len = strlen(resp);
				/* now get rid of everything back to : */
				while(resp[len-1] != ':') {
					resp[len--] = '\0';
				}
				if((tmp = malloc(sizeof(POverview))) == NULL) {
					error_log(ERRLOG_REPORT, xover_phrases[12], NULL);
					retval = RETVAL_ERROR;
				}
				else if((tmp->header = calloc(sizeof(char), len+1)) == NULL) {
					error_log(ERRLOG_REPORT, xover_phrases[12], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					/* initialize the structure */
					strncpy(tmp->header, resp, len); /* don't copy the nl */
					tmp->header[len] = '\0';
					tmp->next = NULL;
					tmp->field = NULL;
					tmp->fieldlen = 0;
					
					if(curptr == NULL) {
						/* at head of list */
						curptr = tmp;
						killp->xoverview = tmp;
					}
					else {
						/* add to linked list */
						curptr->next = tmp;
						curptr = tmp;
					}
				}
			}
		}
	}
	if(retval != RETVAL_OK) {
		/* free up whatever alloced */
		tmp = killp->xoverview;
		while (tmp != NULL) {
			tmp2 = tmp;
			if(tmp->header != NULL) {
				free(tmp->header);
			}
			free(tmp);
			tmp = tmp2;
		}
		killp->xoverview = NULL;
	}
	if((tmp = killp->xoverview) != NULL) {
		do_debug("--Xoverview.fmt list\n");
		while(tmp != NULL) {
			if(tmp->header != NULL) {
				do_debug("item = %s\n", tmp->header);
			}
			tmp = tmp->next;
		}
		do_debug("--End Xoverview.fmt list\n");
	}
}
/*-------------------------------------------------------------------------------------*/
int chk_a_group(PMaster master, POneKill grp, POverview overv, char **reason) {
	/* return REASON_  if xover matches group */
	/* linein should be at tab after the Message Number */
 
	int match = REASON_NONE;
	unsigned long bytes;
	int lines;
	char tchar;
	static char reasonstr[MAXLINLEN];
	
	pmy_regex ptr;
	POverview tmp;
	*reason = reasonstr;
	
	if(grp->bodybig > 0 || grp->bodysmall > 0) {
		tmp = overv;
		while(tmp != NULL && match == REASON_NONE) {
			if(strcasecmp(tmp->header, "Bytes:") == 0) {
				sscanf(tmp->field, "%lu", &bytes); /* convert ascii to long */
				if((grp->bodybig > 0) && (bytes > grp->bodybig)) {
					match = REASON_HIBYTES;
				}
				else if((grp->bodysmall > 0) && (bytes < grp->bodysmall)) {
					match = REASON_LOWBYTES;
				}
			}
			tmp = tmp->next;
		}
	}
	
	if(match == REASON_NONE && (grp->hilines > 0 || grp->lowlines > 0)) {
		tmp = overv;
		while(tmp != NULL && match == REASON_NONE) {
			if(strcasecmp(tmp->header, "Lines:") == 0) {
				sscanf(tmp->field, "%d", &lines);
				if((grp->hilines > 0) && (lines > grp->hilines)) {
					match = REASON_HILINES;
				}
				else if((grp->lowlines > 0) && (lines < grp->lowlines)) {
					match = REASON_LOWLINES;
				}
			}
			tmp = tmp->next;
		}
	}
	if(match == REASON_NONE) {
		ptr = grp->list;
		while (match == REASON_NONE && ptr != NULL) {
			tmp = overv;
			while( tmp != NULL && match == REASON_NONE) {
				if(strcasecmp(tmp->header, ptr->header) == 0) {
                                        /* we need to null terminate the field and restore it later */
					tchar = tmp->field[tmp->fieldlen];
					tmp->field[tmp->fieldlen] = '\0';
					if(regex_block(tmp->field, ptr, master->debug) == TRUE) {
						match = REASON_HEADER;
						sprintf(reasonstr, "%s-%s%s", xover_reasons[REASON_HEADER], tmp->header, tmp->field);
					}
					tmp->field[tmp->fieldlen] = tchar;
				}
				tmp = tmp->next;
			}
			ptr = ptr->next;
		}
	}
	
	return match;
}
/*---------------------------------------------------------------------------*/
int match_group(char *match_grp, char *group, int debug) {
	/* does match match group?  match may contain wildcards */
	int match = FALSE;
	if(match_grp != NULL && group != NULL) {
		if(debug == TRUE) {
			do_debug("Xover - matching %s against %s\n", match_grp, group);
		}
		
		while( *group == *match_grp && *group != '\0') {
			group++;
			match_grp++;
		}
		if(*match_grp == '\0' || *match_grp == '*') {
                        /* wildcard match or end of string, they match so far, so they match */
			match = TRUE;
		}
		if(debug == TRUE) {
			do_debug("match = %s\n", true_str(match));
		}
		
	}
	return match;
}
