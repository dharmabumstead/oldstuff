#include <stdio.h>      /* file functions & much more... */
#include <stdlib.h>     /* integer converts, etc */
#include <dos.h>        /* For date stuff, etc. */
#include <conio.h>      /* keyboard routines (getch() & kbhit()) */
#include <time.h>       /* for timing functions */
#include <string.h>     /* for string compare operations */
#include <alloc.h>      /* for use of 'coreleft()' function */

#include <vv_sys.h>     /* Vermont Views */
#include <vv_wnkt.h>    /* Vermont Views - windowing routines */
#include <vv_main.h>    /* Vermont Views */

#include "vvdavi.h"     /* AVI program function declares & data structs */

#include "hllapi.h"     /* Header file for HLLAPI functions */
#include "hllapi.inc"   /* Include file of HLLAPI functions */

#include <async.h>      /* For ASYNC comm library functions */

ASYNC * comport[3];      /* Pointer to comm ports */

WINDOWPTR rdrwin[4],     /* Reader window pointers */
   alarmwin,             /* Alarm window pointer */
   msgwin,               /* Message window pointer */
   statuswin,            /* Status window */
   errorwin,             /* Error window */
   xcierrwin[2];         /* Controller error windows */

FILE * logfile,          /* Audit file pointer */
   * countsfile,         /* Counts file pointer */
   * errorfile;          /* Error log file pointer */

struct XCIREC xcidata[4];       /* Structures to hold XCI data */
struct BTRVQREC xciqdata;       /* Data structure for BTRIEVE Q'ing */
struct AVI param;               /* Structure to hold program parameters */
struct COM comset[2];           /* Structure to hold comm settings */

int   rdrwinx[4] = {0,40,0,40},  /* X coords for read windows */
   rdrwiny[4] = {0,0,9,9},       /* Y coords for reader windows */
   rdrwinatt[4] = {              /* Reader window color attributes */
      LHIGHLITE,LHIGHLITE,
      LHIGHLITE,LHIGHLITE},
   rdrtitleatt[4] = {         /* Rdr window title attributes */
      LARROW,LARROW,
      LARROW,LARROW},
   rdrgood[4] = {0,0,0,0},    /* Good reads counts */
   rdrbad[4] = {0,0,0,0},     /* Bad reads counts */
   rdrprox[4] = {0,0,0,0},    /* Prox down counts */
   rdrdown[4] = {0,0,0,0},    /* Flags for reader down */
   rdrbcon[4] = {1,1,1,1},    /* Flag for reader broadcast comm */
   qcount[4] = {0,0,0,0},     /* Queue counters */
   qcounttotal = 0,           /* Total queue counts */
   soundflag = 0,             /* Soundflag 3-way toggle */
   qmax[4],                   /* Max queue count */
   qflag,                     /* Flag for queueing on/off */
   dl10 = -1,                 /* DL10 present - set to -1 at startup */
   error,                     /* Generic error flag */
   xciflag[2] = {0, 0},       /* Flag for XCI controller present */
   errorflag[2] = {0, 0},     /* Controller error flags */
   hllapiflag = 0,            /* HLLAPI on/off flag */
   garbcount[2] = {0,0},      /* Garbage counter */
   dosbug = 0,                /* Day flag for DOS date error handler */
   initday,                   /* Startup day */
   initmonth,                 /* Startup month */
   inityear,                  /* Startup year */
   initcentury,
   monthdays[] = {0,31,28,31, /* Number of days in each month */
      30,31,30,31,31,         /* (0 for element 0 in array)   */
      30,31,30,31},           /* used to correct DOS date bug */
   sendcount,                 /* Total number of sends to broadcast */
   dl10dbug = 0;              /* DL10 mode flag */

float avgtime;                /* Average time to broadcast */

char  API_String[255],          /* Buffer for HLLAPI calls */
   utility[80],                 /* All-purpose work buffer */
   bcsend[30],                  /* Work buffer for BCPC sends */
   pos_blk[128],                /* Buffer for BTRV queue file */
   err_blk[128],                /* Buffer for BTRV error log */
   errkey[2],                   /* Key 0 for BTRV error file */
   qkey0[19],                   /* Lookup key 0 for BTRV Q record */
   qkey1[3],                    /* Lookup key 1 for BTRV Q record */
   rdrtitle[4][36],             /* Reader window titles */
   * msgwintitle = "MESSAGES",  /* Message window title */
   * version =	"AVI/PC v1.00 10/23/91 by JS Butler",  /* Version string */
   lastgood[4][30],             /* Last good read array for no reads */
   instring[2][30],             /* Input buffers for comm ports */
   outstring[30];               /* Output buffers for comm ports */

#include "turcbtrv.c"           /* Include file for BTRIEVE interface */

main(void) {
   int i,t;                     /* General purpose integers */
   char userkey;                /* Holder for user keystrokes */
   unsigned long memleft;       /* For memory status */

   init_vv();                   /* Initialize Vermont Views system */

   printf("%s\n",version);      /* Show version on startup */

   getcfginfo();                /* Get info from config file */

   initstartday();
   initstartmonth();
   initstartyear();
   initstartcentury();

   drawscreen();                /* Draw the screen! */
   showavgtime();            /* Show status line info */

   openlogfile();               /* Open the audit file */
   opencountsfile(0);           /* Open counts file */
   openerrlog();                /* Open the error log file */

   sprintf(utility, "AVI/PC: %s program startup", param.mname);
   logerror(utility,"O");       /* Put startup info in ERROR.LOG */

   error = openqfile(t);        /* Open the BTRV files for queueing */

   if(error != 0) {             /* Problems? */
      sprintf(utility, "AVI/PC: BTRV Error %i opening queue file %i.",
         error, t);
      outmessage(utility);    /* Display the message */
      logerror(utility,"O");  /* Log the message in ERROR.LOG */
   }

   gotoxy(1,1);               /* Reset cursor to upper left */
   csr_hide();                /* Hide the cursor */

	
   for(t=0; t<2; t++) {       /* Check to see what's on each port */
      if(strstr(comset[t].device,"XCI") != NULL) {    /* XCI on port */
         initasync(t);           /* Initialize port as XCI */
	 xciflag[t] = 1;         /* Set the XCI flag for this port */
	 checkxcistatus(0,t);    /* Reset XCI message timer for this port */
      } else {                   /* If it's NOT an XCI controller */
	 if(strstr(comset[t].device,"DL10") != NULL) {   /*  DL10 on port */
	    dl10 = t;            /* Set DL10 flag */
	    initdl10(t);         /* Initialize com port as DL10 */
	 }
      }
   }

   if(param.dl10com3 == 1) {      /* If DL10 is on COM3: */
      initdl10(2);                /* Initialize it as such */
      dl10 = 2;                   /* Set DL10 flag */
   }

   showdl10("SYSTEM INIT - WAIT"); /* Show on DL10 */

   if ((error = (get3270())) != 0) {  /* If can't connect to HLLAPI session */
      msgalert();                 /* Turn messages yellow on red */
      sprintf(utility, "BCPC: Error %i connecting to 3270!");
      outmessage(utility);        /* Show error message */
      alarm(1,0);                 /* Sound 1 alarm (no flash) */
      logerror(utility, "C");     /* Log error */
      msgnormal();                /* Messages back to normal */
      if (wn_isup(statuswin) != 0) hidestatus(); /* Hide status window if up */
   } else {                      /* Connected OK */
      hllapiflag = 1;            /* Set HLLAPI flag */
      outmessage("BCPC: Connected to 3270"); /* Show message */
   }

   if(param.bcautologon == 1) broadcastlogon(); /*  logon to broadcast */

	showavgtime();               /* Update status line */
   showdl10("AVI/PC v1.35 READY"); /* Show DL10 message */


   while(userkey != 27) {          /* Do until user hits ESCape */
      while(!kbhit()) {            /* If no key is pressed */
         showsystime();            /* Show the current time */
	 for(t=0;t<2;t++) {        /* Repeat twice (2 com ports) */
	    if(xciflag[t] == 1) {     /* If XCI on this com port */
	       xcihandler(t);         /* Call XCI handler routine */
	    }
	 }
      }

      userkey = getch();              /* A key was hit, so get it */
      switch(userkey) {               /* See what it is */
	 case 'c':                    /* Clear the counts */
	    error = getpassword(15, param.password);  /* Get system password */
	    if(error == 0) {          /* If password OK */
	       clearcounts();         /* Clear counts */
	       outmessage("USER: Counts cleared");
	       logerror("USER: Counts cleared","O");
	    }
	    break;

      case 'l':                   /* Toggle DL10 mode (normal/debug) */
	    if(dl10dbug==0) {         /* If now NORMAL (0) */
            dl10dbug = 1;         /* Flip it to DEBUG (1) */
				outmessage("USER: DL10 now in debug mode"); /* Show on screen */
            showdl10("DL10 DEBUG MODE");  /* Show on DL10 */
            break;                 /* Get out of case statement */
         } else {                  /* If its in DEBUG */
            dl10dbug = 0;          /* Set to NORMAL */
            outmessage("USER: DL10 now in normal operation");
            showdl10("DL10 NORMAL MODE");  /* Show on DL10 */
            break;                 /* Get out of CASE statement */
         }
      case 'q':                   /* Toggle sound flag */
         if(soundflag == 0) {     /* If at first toggle position */
            soundflag = 2;        /* Wrap around to last toggle position */
         } else {                 /* Otherwise... */
            soundflag--;          /* Decrement toggle position */
         }

         switch(soundflag) {      /* Assemble appropriate message */
            case 0:
               sprintf(utility,"USER: Sound and flash now ON");
               break;
            case 1:
               sprintf(utility,"USER: Sound is off, flash is on");
               break;
            case 2:
               sprintf(utility,"USER: Sound and flash now OFF");
               break;
            default:
               break;
         }
         outmessage(utility);     /* Show message on screen */

			showavgtime();        /* Update status line */
         alarm(1,1);              /* Alarm with 1 beep & 1 flash */
         break;                   /* Break out of case statement */
      case 'b':                   /* Toggle broadcast */
         error = getpassword(15, param.password); /* Get system password */

         if(error == 0) {         /* If it's OK */
            if(param.bccomm == 1) {  /* If broadcast now on */
               msgnormal();          /* Normal messages */
               param.bccomm = 0;     /* Set broadcast flag */
               outmessage("USER: Broadcast communications now OFF!");
               logerror("USER: Broadcast communications now OFF!","C");
            } else {                 /* Otherwise... */
               param.bccomm = 1;     /* Set broadcast flag */
               outmessage("USER: Broadcast communications now ON!");
               logerror("USER: Broadcast communications now ON!","C");
            }
				showavgtime();        /* Update status line */
         }
         break;                      /* Break out of functon */
      case 'x':                      /* Toggle HLLAPI connection */
         error = getpassword(15, param.password); /* Get system password */
         if(error == 0) {            /* If it's OK */
            if(hllapiflag == 1) {    /* If HLLAPI's now ON */
               msgalert();           /* Set message output to alert colors */
               outmessage("USER: 3270 HLLAPI link cut!"); /* Show message */
               alarm(1,0);           /* Alarm with 1 beep and no flashes */
               logerror("USER: 3270 HLLAPI link cut!", "C"); /* Log it */
               drop3270();           /* Call function to cut 3270 */
               hllapiflag = 0;       /*  Reset HLLAPI flag */
               msgnormal();          /* Reset message colors */
            } else {                 /* If HLLAPI's now OFF */
               if((error = get3270()) == 0) {  /* If restored OK */
                 outmessage("USER: 3270 HLLAPI link restored");
                 logerror("USER: 3270 HLLAPI link restored","C");
                 hllapiflag = 1;     /* Set flag back to 1 */
               } else {              /* Error restoring HLLAPI */
                  msgalert();        /* Alert messages */
                  sprintf(utility,
		     "USER: 3270 HLLAPI link NOT restored! (Error %i)",
		     error);
                  outmessage(utility); /* Show error message */
                  alarm(1,1);        /* Sound alarm & flash */
                  logerror(utility,"C"); /* Log error message */
                  msgnormal();       /* Reset message colors */
               }
            }
         }
         break;
      case 'm':                   /* Display free memory */
         memleft = coreleft();    /* Get the amount of mem left */
         sprintf(utility, "USER: System memory free %lu", memleft);
         outmessage(utility);     /* Show it */
         break;                   /* Done */
      case 'v':                   /* Display version information */
         sw_att(LHIGHLITE, msgwin);  /* Change window output colors */
         sw_bdratt(LDEBUGMSG, msgwin);
         wn_upd(msgwin);          /* Update the window */
         outmessage(version);     /* Show the version string */
         msgnormal();             /* Reset the message colors */
         break;                   /* Done */
      case 'd':                   /* Test DL10 display, if attached */
         testdl10();              /* Call function */
         break;                   /* Done */
      case 's':                   /* User wants a status check */
         outmessage("USER: Status check");
         for(t=0; t<2; t++) {     /* Once for each com port */
            if(xciflag[t] == 1) { /* Does it have an XCI controller? */
               a_putc('r',comport[t]);  /* Send an 'r' to XCI to get status */
            }
         }
         break;                   /* End case statement */
      case '1':                   /* Toggle broadcast comm for readers */
         rdrtoggle(0);            /* Call function */
         break;
      case '2':
         rdrtoggle(1);
         break;
      case '3':
         rdrtoggle(2);
         break;
      case '4':
         rdrtoggle(3);
         break;
      case 27:                    /* ESC key - exit program */
         error = getpassword(15, param.password); /* Get system password */
         if(error == 0) {         /* If it's OK */
            outmessage("USER: Password OK - closing down");
            logerror("USER: Password OK - closing down","O");
            msgnormal();
            break;
         } else {
            if(error != 0) userkey = ' '; /* Reset key buffer */
            break;
         }
      default:                    /* Unrecognized key */
         alarm(1,0);              /* Beep */
      }
   }


   if(param.bccomm != 0) {
      showstatus("AVI/PC STATUS");   /* Put up status window */
      statusmsg("Logging off....");           /* Show status message */
      sendkeystrokes("logoff",param.hkeydelay);       /* Send to broadcast */
      hidestatus();                           /* Remove status window */
   }

   drop3270();                    /* Drop HLLAPI connection */
   closeerrlog();                 /* Close the error log */
   cleardl10();                   /* Clear DL10 display */
   undrawscreen();                /* Remove windows and erase screen */
   a_close(comport[0],0);         /* Close COM1: */
   a_close(comport[1],0);         /* Close COM2: */

   if(param.dl10com3 == 1) {      /* If DL10 on COM3: */
      a_close(comport[3],0);      /* Close it */
   }

   closeqfile();                  /* Close BTRV queue file */
   updatecountsfile();            /* Write counts */
   flushall();                    /* Close all open files */
   exit_vv();                     /* Release Vermont Views */
   clrscr();                      /* Clear the screen */
   exit(0);                       /* That's all folks! */
   return(0);                     /* To shut the compiler up */
}

/***************************************************************************/
/*                          AVI/PC ROUTINES                                */
/***************************************************************************/
void clearcounts() {
   /*********************************************************************/
   /* Clears counts and updates screen and counts file                  */
   /*********************************************************************/
   int t;                         /* Counter */

   for(t=0;t<4;t++) {             /* Repeat once for each reader */
      rdrgood[t] = 0;             /* Clear good reads */
      rdrbad[t] = 0;              /* Clear bad reads */
      rdrprox[t] = 0;             /* Clear prox counts */
      showcounts(t);              /* Show new counts */
   }
   updatecountsfile();            /* Write cleared counts to counts file */
}

int rdrtoggle(int w) {
   /*********************************************************************/
   /* PURPOSE: Toggles broadcast communications for a reader            */
   /*  PARAMS: int w - reader to toggle                                 */
   /* RETURNS: 0 if successful or 1 if reader not installes             */
   /*********************************************************************/

   if (strstr((char *) rdrtitle[w],"UNUSED") != NULL) { /* No reader here! */
      sprintf(utility, "USER: Reader %i is not installed!", w);
      outmessage(utility);        /* Show error message */
      alarm(1,0);                 /* Beep */
      return(1);                  /* Return error */
   }

   if((error = getpassword(15,param.password)) == 0) { /* Get password */
      if(rdrbcon[w] != 0) {       /* If broadcast is on for reader */
         rdrbcon[w] = 0;          /* Turn it off */
         rdrwinatt[w] = LFLDACT;  /* Change color attribute for reader win */
         changewindow(w,rdrwinatt[w]); /* Change reader window */
         sprintf(utility, "USER: Reader %i broadcast OFF", (w+1));
         outmessage(utility);     /* Show message */
         return(0);               /* Return OK */
      } else {                    /* If reader is off */
         rdrbcon[w] = 1;          /* Turn it on */
         rdrwinatt[w] = LHIGHLITE;     /* Change window color attribute */
         changewindow(w,rdrwinatt[w]); /* Change window */
         sprintf(utility, "USER: Reader %i broadcast ON", (w+1));
         outmessage(utility);     /* Show message */
      }
   }
   updatecountsfile();            /* Store change in counts file */
   return(0);                     /* Return OK */
}

int isprintstr(char * teststring) {
   /*********************************************************************/
   /* PURPOSE: Sees if string has no unprintable characters in it       */
   /*  PARAMS: char * testring - pointer to string to test              */
   /* RETURNS: 1 if unprintable characters or 0 if OK                   */
   /*********************************************************************/

   int  t,                        /* Counter */
	slen;                     /* String length */

   slen = strlen(teststring);     /* Get string length */

   for(t=0; t<slen; t++){         /* Repeat for length of string */
      if (!isprint(teststring[t])) return(1); /* Check each character      */
   }                                          /* and return if unprintable */
   return(0);                     /* Return OK if all chars check out */
}

int getpassword(int passtime, char * password) {
   /*********************************************************************/
   /* PURPOSE: gets a password within an alloted number of seconds      */
   /*  PARAMS: int passtime - number of seconds to wait for password    */
   /*          char * password - password to wait for                   */
   /* RETURNS: 0 if password is correct, 1 if not, 2 if time expired    */
   /*********************************************************************/
   clock_t start, end;            /* Start & end time holders */
   char temp;                     /* Holds user keystroke */
   int counter = 0,               /* Keystroke counter */
   countlen = strlen((char *) password);  /* Length of password */
   float etime;                   /* Elapsed time */

   msgalert();                    /* Alert messages */
   sprintf(utility,"USER: Type password within %i seconds", passtime);
   outmessage(utility);           /* Prompt user */

   start = clock();               /* Store start time (clock ticks) */

   do {                           /* Repeat loop */
      end = clock();              /* Get current time in clock ticks */
      gotoxy(1,1);                /* Move cursor */
      etime = ((end-start)/18.2); /* Calculate elapsed time in seconds */
      if(kbhit()) {               /* If a key was hit */
         temp = getch();          /* Get the key */
         if(temp != password[counter++]) {
	    /* Compare it with password array character and advance */
            /* counter                                              */
            outmessage("USER: Password incorrect.");  /* Show message */
            logerror("USER: Password incorrect.","O");  /* Log error */
            msgnormal();          /* Normal message color */
            return(1);            /* Return error message */
         }
      }
   } while((etime < passtime) && (counter < countlen));
   /* Repeat while elapsed time is less than wait time and number of     */
   /* keystrokes is less than length of password (no need to hit RETURN) */

   if(counter==countlen) {        /* If entered full length of password */
      msgnormal();                /* Normal message color */
      return(0);                  /* Return OK */
   } else {                       /* Otherwise, time ran out! */
      outmessage("USER: 15 seconds elapsed....try again....");
      logerror("USER: 15 seconds elapsed....try again....","O");
      msgnormal();                /* Normal messages */
      return(2);                  /* Return 2 for timeout error */
   }
}

int validate(char * string, int valtype) {
   /*********************************************************************/
   /* PURPOSE: Checks a string to see if it's all number or letters     */
   /*  PARAMS: char * string - string to test                           */
   /*          int valtype - 0 for numeric, 1 for alpha check           */
   /* RETURNS: 1 if check fails, 0 if checks ok                         */
   /*********************************************************************/

   int t;                         /* Counter */

   switch(valtype) {
      case 0:                     /* Test string for numbers only */
         for(t=0; t<(strlen(string)); t++) { /* Check each character */
            if(!(isdigit(string[t]))) return(1);  /* Return if not numeric */
         }
         break;                   /* Break out of case */
      case 1:                     /* Test string for alpha only */
         for(t=0; t<(strlen(string)) ;t++) {  /* Check each character */
            if(!(isalpha(string[t]))) return(1); /* Return if non alpha */
         }
         break;                   /* Break out of function */
   }
   return(0);                     /* Return OK */
}

void getdatestr(char * string) {
   /*********************************************************************/
   /* PURPOSE: Place today's date in mm/dd/yy format in string          */
   /*  PARAMS: char * testring - pointer to buffer to put date in       */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date nowdate;           /* Data structure for current time */
   int month, day, year;          /* Whaddya think? */

   getdate(&nowdate);             /* Get today's date */
   day = nowdate.da_day;          /* Break down date into integers */
   month = nowdate.da_mon;
   year = nowdate.da_year;

   sprintf(string,                       /* Put in string buffer.  As an     */
      "%02i/%02i/%02i",                  /* example, February 14, 1991 would */
        month,day,(year-initcentury));   /* be '02/14/91'*/
}


void gettimestr(char * string) {
   /*********************************************************************/
   /* PURPOSE: Place current time in HH:MM:SSXM format in string        */
   /*  PARAMS: char * string - pointer to buffer to put time in         */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct time nowtime;           /* Data structure for current time */
   int hour, minute, second;      /* Whaddya think? */

   gettime(&nowtime);             /* Get the current time */
   hour = nowtime.ti_hour;        /* Breakout hour */
   minute = nowtime.ti_min;       /* Breakout minute */
   second = nowtime.ti_sec;       /* Breakout second */

   if(hour > 12) {                /* If hour is larger than 12 */
      sprintf(string,"%02i:%02i:%02iPM",
         (hour-12),minute,second);     /* Example: 01:12:43PM */
   } else {
      if(hour <12) {              /* If hour is smaller than 12 */
         sprintf(string,"%02i:%02i:%02iAM",
			   hour,minute,second);  /* Example 10:48:11AM */
      } else {                    /* If hour equals 12 */
         sprintf(string,"%02i:%02i:%02iPM",
			   hour,minute,second);  /* Example 12:24:38pm */
      }
   }
}

int openlogfile() {
   /*********************************************************************/
   /* PURPOSE: Opens the audit log file 'AUDITnn.LOG', where nn is the  */
   /*          number of the current month.  For example, if today were */
   /*          January 1, 1992, we'd open 'AUDIT01.LOG'.                */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date logdate;           /* Data structure for today's date */
   int month;                     /* Integer for month number */
   char filename[14];             /* Buffer for filename */

   getdate(&logdate);             /* Get today's date */
   month = logdate.da_mon;        /* Get the month number */

   /* Prepare the filename: AUDIT##.LOG, where ## is the month number */
   sprintf(filename,"AUDIT%02i.LOG",month);        /* Prepare the filename */
   sprintf(utility, "AVI/PC: Opening audit file %s", filename);
   outmessage(utility);           /* Show message */
   if ((logfile = fopen(filename, "at")) == NULL) {  /* If can't open file */
      msgalert();                 /* Alert message colors */
      outmessage("AVI/PC: Error opening log file!");  /* Show message */
      alarm(2,1);                 /* Alarm twice with 1 flash */
      logerror("AVI/PC: Error opening log file!", "O"); /* Log it */
      msgnormal();                /* Normal messages */
      return(1);                  /* Bail out of function with error */
   }
      return(0);                  /* Return from function */
}

int closelogfile() {
   if((error = fclose(logfile)) != 0) {
	   sprintf(utility, "AVI/PC: Error %i closing log file",error);
      msgalert();
      outmessage(utility);
      logerror(utility, "O");
      msgnormal();
      return(error);
   }

   return(0);
}

int opencountsfile(int flag) {
   /*********************************************************************/
   /* PURPOSE: Opens the counts file and reads in the counts for all    */
   /*          readers, as well as the current state of the broadcast   */
   /*          communications, the sound toggle, and the queue counts.  */
   /*  PARAMS: flag (this was to clear the counts - obsolete)           */
   /* RETURNS: 0 if successful or 1 is there's an error                 */
   /*********************************************************************/
   char  filename[14],            /* Buffer for filename */
         temp1[8],                /* Temp strings */
         temp2[8],
         temp3[8];

   int   t;                       /* Integer for counter */

   sprintf(filename,"COUNTS.FIL"); /* Load filename buffer */
   if ((access(filename,0) == 0) && (flag == 0)) {    /* Does file exist? */
      if ((countsfile = fopen(filename, "r+")) == NULL) {  /* If can't open */
         msgalert();
         outmessage("AVI/PC: Error opening counts file!");  /* Show error */
         logerror("AVI/PC: Error opening counts file!","O"); /* Log error */
         msgnormal();
         return(1); /* Return error code */
      }
      outmessage("AVI/PC: Retrieving counts");
      for(t=0;t<4;t++) {                    /* Once for each reader */
         fgets(temp1, 8, countsfile);       /* Get line with good reads */
         fgets(temp2, 8, countsfile);       /* Get line with bad reads */
         fgets(temp3, 8, countsfile);       /* Get line with prox count */
         rdrgood[t] = atoi((char *) temp1); /* Convert goods to integer */
         rdrbad[t] = atoi((char *) temp2);  /* Convert bads to integer */
         rdrprox[t] = atoi((char *) temp3); /* Convert prox count to integer */
         if (strstr((char *) rdrtitle[t],"UNUSED") == NULL) {
            /* If reader installed here, show counts */
            showcounts(t);
         }
      }

      for(t=0;t<4;t++) {               /* Get queue counts for each reader */
         fgets(temp1,8,countsfile);    /* Get line from counts file */
         qcount[t] = atoi((char *) temp1);       /* Convert line to int */
         qcounttotal = qcounttotal + qcount[t];  /* Add total queue count */
      }

      for(t=0;t<4;t++)  {           /* Get reader broadcast comm status */
         fgets(temp1,8,countsfile); /* Get line from file */
         rdrbcon[t] = atoi((char *) temp1);   /* Convert to integer */
         if(rdrbcon[t] != 0) {      /* If it's broadcast is on for reader */
            if (strstr((char *) rdrtitle[t],"UNUSED") == NULL) {
               /* If a reader is installed here the reader title */
               /* shouldn't have "UNUSED" in it!                 */
               rdrwinatt[t] = LHIGHLITE;     /* Set the reader window color */
               changewindow(t,rdrwinatt[t]); /* Change reader window */
            }
         } else {   /* If broadcast is off for this reader */
            if (strstr((char *) rdrtitle[t],"UNUSED") == NULL) {
               /* If a reader is installed here */
               rdrwinatt[t] = LFLDACT;        /* Change window attribute */
               changewindow(t,rdrwinatt[t]);  /* Change reader window */
            }
         }
      }

      fgets(temp1,8,countsfile);         /* Get line from file */
      soundflag = atoi((char *) temp1);  /* Convert & store as soundflag  */

   } else {                       /* If file doesn't exist, create & open */
      if ((countsfile = fopen(filename, "w+")) == NULL) { /* File error ? */
         msgalert();
         outmessage("AVI/PC: Error opening counts file!");   /* Show error */
         logerror("AVI/PC: Error opening counts file!","O"); /* Log error */
         msgnormal();
         return(1);               /* Return error code */
      } else {
         for(t=0; t<4; t++) {     /* Once for each reader window */
            if (strstr((char *) rdrtitle[t],"UNUSED") == NULL) {
               /* If reader is installed for window */
               showcounts(t);     /* Show the counts */
            }
         }
      }
   }

   if(qcounttotal != 0) qflag = 1;  /* Turn queue flag on if queues exist */

   return(0);                       /* Return with no error */
}

int updatecountsfile() {
   /*********************************************************************/
   /* PURPOSE: Updates counts file with current counts                  */
   /*  PARAMS: none                                                     */
   /* RETURNS: 0 if successful or 1 if error writing to file            */
   /*********************************************************************/

   int t;                         /* Counter integer */

   if(countsfile == NULL)
      return(1);                  /* If file isn't open, abort function */

   rewind(countsfile);            /* Move file pointer to beginning of file */

   for(t=0;t<4;t++) {             /* Repeat once for each reader */
      /* Write good reads, bad reads, & prox counts to file */
      fprintf(countsfile,"%i\n%i\n%i\n",rdrgood[t],rdrbad[t],rdrprox[t]);
   }

   for(t=0;t<4;t++) {             /* Repeat once for each reader */
      fprintf(countsfile,"%i\n",qcount[t]);   /* Write queue counts */
   }

   for(t=0;t<4;t++) {             /* Repeat once for each reader */
      /* Write broadcast communications toggles for each reader */
      fprintf(countsfile,"%i\n",rdrbcon[t]);
   }

   fprintf(countsfile,"%i\n",soundflag);  /* Write sound toggle */

   return(0);                     /* Return from function */
}

int logtrans(int rdrnum) {
   /*********************************************************************/
   /* PURPOSE: Logs current tag read to audit file                      */
   /*  PARAMS: logical reader number to log for (11, 31, etc)           */
   /* RETURNS: 0 if successful or 1 if error writing to file            */
   /*********************************************************************/

   char    msg[255];              /* Buffer for message */

   /* Print record to a string prior to writing to audit file */
   sprintf(msg,"%s,%s,%02i%02i%02i,%02i%02i%02i,%s,%s,%s",
      xcidata[rdrnum].readerport, xcidata[rdrnum].tagid,
      xcidata[rdrnum].year,xcidata[rdrnum].month, xcidata[rdrnum].day,
      xcidata[rdrnum].hour,xcidata[rdrnum].minute,xcidata[rdrnum].second,
      xcidata[rdrnum].samereads,xcidata[rdrnum].goodreads,
      xcidata[rdrnum].dblevel);

   if(logfile != NULL) {              /* If audit file is open */
      fprintf(logfile, "%s\n", msg);  /* Write the record to file */
      return(0);                      /* Return from function */
   } else {                           /* If audit file ISN'T open */
      return(1);                      /* Bail out of function with error */
   }

   return(0);                         /* Return with no errors */
}


int stripnewline(char * string) {
   /*********************************************************************/
   /* PURPOSE: Strips newline character from a character string         */
   /*  PARAMS: none                                                     */
   /* RETURNS: 0                                                        */
   /*********************************************************************/

   int t;                            /* Counter integer */

   for(t=0;t<(strlen(string));t++) { /* Repeat for length of string */
      /* If newline character, terminate string by inserting ASCII 0 */
      if(string[t] == '\n') string[t] = '\0';
   }

   return(0);                        /* Return from function */
}


int showxcierrwin(int com) {
   /*********************************************************************/
   /* PURPOSE: Shows error window if XCI controller goes down           */
   /*  PARAMS: Logical controller number (0 or 1)                       */
   /* RETURNS: 0                                                        */
   /*********************************************************************/

   int   t;                    /* counter */
   char  title[80],            /* Buffer for window title */
         message[80],          /* Message buffer */
         nowtime[11],          /* Buffer for time */
         nowdate[9];           /* Buffer for date */

   if wn_isup(xcierrwin[com])  /* Is it already up for some reason? */
      return(1);               /* If it is, return with an error */

   gettimestr(nowtime);        /* Get the current time and store in nowtime */
   getdatestr(nowdate);        /* Get the current date and store in nowdate */

   sprintf(title, " COMMUNICATIONS PORT %i ERROR AT %s ON %s! ",
      (com+1), nowtime, nowdate);     /* Prepare error message */
   alarm(3,1);                        /* Sound alarm & flash */

   sw_title(title, LDEBUGMSG, TOPCENTER,
      xcierrwin[com]);                /* Change window's title attribs */

   wn_expset(xcierrwin[com]);         /* Explode window up */

   sprintf(message, "XCI CONTROLLER '%s' AT '%s' IS DOWN!",
      (char *) comset[com].comctrldesc,
      (char *) comset[com].ctrlloc);  /* Prepare message */

   /* Show message in center of window */
   v_stpl(3, CENTER_TEXT, message, xcierrwin[com]);  

   return(0);                         /* End function */
}


int hex2dec(char * string) {
   /*********************************************************************/
   /* PURPOSE: Converts a hexadecimal number to a decimal integer       */
   /*  PARAMS: char * string - string containing hex number             */
   /* RETURNS: integer containg decimal number                          */
   /*********************************************************************/

   int multi;                     /* Multiplier */
   strupr(string);                /* Convert hex string to uppercase */

   if(isupper(string[1])) {       /* If second char is A thru F */
      /* Convert to decimal by subtracting from ASCII value */
      multi = ((int) string[1] - 55);
   } else {                      /* If not... */
      /* subtract ASCII value to get decimal value */
      multi = ((int) string[1] - 48);  
   }

   if(isupper(string[0])) { /* If first char is A thru F....*/
      /* Convert ASCII char to decimal & multiply by 16,
         adding value of second char */
      multi = multi + (((int) string[0] -55) *16);
   } else {
      /* If number, convert ASCII to decimal number & multiply * 16,
         adding value of second char */
      multi = multi + ((int) string[0] - 48) * 16;
   }

   return(multi);         /* Return decimal value */
}

void stathandle(int com) {
   /*********************************************************************/
   /* PURPOSE: Handles status messages from XCI controller              */
   /*  PARAMS: com port status message came in on (0 or 1)              */
   /* RETURNS: 0                                                        */
   /*********************************************************************/

   int  statint,         /* Integer for hex status checksum */
        t,               /* counter */
        winno,           /* logical reader window number to update */
        multi = 1;       /* multiplier for bit operations */

   char    statmsg[5];

   strmid(instring[com], 4, 2, statmsg);
   statint = hex2dec(statmsg);

   for(t=0;t<2;t++) {
      if((comset[com].reader[t].rdrnum !=0) && ((statint & multi) != multi)) {
         /* If reader exists and bit for reader is off */
         rdrdown[((com+com)+t)] = 1;         /* Set flag for reader down */
         changewindow((com+com)+t,LDEBUG);   /* change reader window to red */

	 sprintf(utility, "XCI %i: Reader %i down!",(com+1), (t+1));
         msgalert();             /* Alert message output */
         outmessage(utility);    /* show message */

         /* If reader down alarm is set for this reader, sound alarm */
         if(comset[com].reader[t].rdrdownalarm == 1)
            alarm(5,1);

         logerror(utility, "E"); /* Log error in file */
         msgnormal();            /* Return to normal output message color */

         if(param.bccomm == 1 && rdrbcon[((com+com)+t)] == 1) {
            /* If broadcast communication for AVIPC & reader are on */
            error = getportnum(com, t);  /* Get the port number (11, 31 etc) */

            sprintf(bcsend,"BCPC%iDOWN %.3sRT",   /* prepare BCPC message */
               error,comset[com].reader[t].rdrloc);

            sendbcpc(bcsend); /* send it */
         }

      } else {        /* if checksum from XCI shows reader is attached */
         if(rdrdown[((com+com)+t)] == 1) {  /* if reader down flag is set */

            /* change the reader window back to normal         */

            /* ((com+com)+t) will calculate the array offset
               example: reader 0 (the first reader) on com 2
               (known to program as 1 in 'com' variable) would
               calculate:   1+1+0=2  so we now have an array
               offset.  For example: rdrwin[2].                */

            changewindow(((com+com)+t), rdrwinatt[((com+com)+t)]);

            rdrdown[((com+com)+t)] = 0; /* reset the reader down flag */

            /* prepare message for display and error log */
            sprintf(utility, "XCI %i: Reader %i now online",
               (com+1), (t+1));

            outmessage(utility);       /* Show message in message window */
            logerror(utility,"E");     /* Log message to error log file */

            error = getportnum(com,t); /* Get BCPC port number */
            sprintf(bcsend,"BCPC%iUP   %.3sUT",
               error,comset[com].reader[t].rdrloc);  /* prepare BCPC message */

            if(param.bccomm == 1 && rdrbcon[((com+com)+t)] == 1) {
               /* If AVIPC & reader broadcast communications are on */
               sendbcpc(bcsend);       /* send it */
            }
         }
			msgnormal();               /* Reset msg window output to default */
      }
      multi = (multi + multi) * 2;  /* add to multi for second comm port */
   }
}


int getportnum(int comport, int rdrport) {
   /*********************************************************************/
   /* PURPOSE: Returns a BCPC port number for a reader based on which   */
   /*          comm port & reader port it's on                          */
   /*  PARAMS: int comport - which comport (0 or 1) it's on             */
   /*          int rdrport - which reade port (0 or 1) it's on          */
   /* RETURNS: the port number for BCPC (11, 31, 21 or 41)              */
   /*********************************************************************/

   /* COM1: Reader 1 */
   int portnum = 11;

   /* COM1: Reader 2 */
   if((comport == 0) && (rdrport == 1)) portnum = 31;

   /* COM2: Reader 1 */
   if((comport == 1) && (rdrport == 0)) portnum = 21;

   /* COM2: Reader 2 */
   if((comport == 1) && (rdrport == 1)) portnum = 41;

   return(portnum);       /* Return the port number */
}

int alarm(int number, int flash) {
   /*********************************************************************/
   /* PURPOSE: Sounds an alarm & optionally flashes message window      */
   /*  PARAMS: int number - number of times to sound alarm              */
   /*          int flash - 1 for flash or 0 for no flash                */
   /* RETURNS: always 0                                                 */
   /*********************************************************************/

   int t;                         /* Counter */

   if(soundflag == 2) return(0);  /* If no sound or flash return */

   for(t=0;t<number;t++) {        /* Repeat for number */
      if(flash == 1) {            /* If flash flag is on... */
         /* Flash window */
         sw_att(LDEBUG,alarmwin);
         if(wn_isup(alarmwin)!=0) wn_dn(alarmwin);
         wn_up(alarmwin);
         if(soundflag == 1) time_delay(7);
      }

      if(soundflag == 0) beep_vv(7, 1000);  /* If sound flag on, beep */

      if(flash == 1) {            /* If flash flag is on */
         /* Flash over message window */
         sw_att(LREVERSE,alarmwin);
         wn_dn(alarmwin);
         wn_up(alarmwin);
         if(soundflag == 1) time_delay(7);  /* Pause flash if no sound */
      }

      if(soundflag == 0) beep_vv(7, 800);   /* If sound flag, beep */
   }

   /* Repeat the same process, but with lower tone */

   if(flash == 1) {
      for(t=0;t<3;t++) {
         sw_att(LDEBUG,alarmwin);
         if(wn_isup(alarmwin)!=0) wn_dn(alarmwin);
         wn_up(alarmwin);
         time_delay(7);
         sw_att(LREVERSE,alarmwin);
         wn_dn(alarmwin);
         wn_up(alarmwin);
         time_delay(7);
      }
   }

   if(wn_isup(alarmwin)!=0) wn_dn(alarmwin);
   return(0);
}

int getcfginfo() {
   /*********************************************************************/
   /* PURPOSE: Retrieves data from config file & stores in structures   */
   /*  PARAMS: none                                                     */
   /* RETURNS: 0 if successful or 1 if error                            */
   /*********************************************************************/

   int t, multi, loop1, loop2, loop3;
   char cfgparam[30];             /* Buffer for lines from file */
   FILE * cfg;                    /* File pointer */

   if((cfg = fopen("NETAVI.CFG","rt")) == NULL) {
      /* If error opening file, abort the program */
      printf("FATAL ERROR! Could not access NETAVI.CFG!\n");
      exit(1);
   }

   /* Now, parse it out into the appropriate structures */

   /* COM1 settings */
   fgets((char *)cfgparam,30,cfg); /* Device type XCI or DL10 */
   strncpy((char *) comset[0].device, (char *) cfgparam, 10);

   fgets((char *)cfgparam,30,cfg);   /* Baud rate for port */
   comset[0].baud = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Parity */
   comset[0].parity = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Word length */
   comset[0].wordlen = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Stop bits */
   comset[0].stop = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Controller description */
   strncpy((char *) comset[0].comctrldesc, (char *) cfgparam,5);
   stripnewline((char *) comset[0].comctrldesc);

   fgets((char *)cfgparam,30,cfg); /* Controller location */
   strncpy((char *) comset[0].ctrlloc, (char *) cfgparam, 5);
   stripnewline((char *) comset[0].ctrlloc);

   fgets((char *)cfgparam,30,cfg); /* Slow threshold */
   comset[0].slowthresh = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Alarm flag for slow controller */
   comset[0].slowalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Alarm flag for down controller */
   comset[0].downalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg); /* Alarm flag for stat change */
   comset[0].rdrstatcalarm = atoi((char *) cfgparam);

   /* COM1 Reader setups */
   /* Reader 1 */
   fgets((char *)cfgparam,30,cfg);   /* Reader number */
   comset[0].reader[0].rdrnum = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader location */
   strncpy((char *) comset[0].reader[0].rdrloc, (char *) cfgparam, 10);
   stripnewline((char *) comset[0].reader[0].rdrloc);

   fgets((char *)cfgparam,30,cfg);   /* Reader description */
   strncpy((char *) comset[0].reader[0].rdrdesc, (char *) cfgparam, 35);
   stripnewline(comset[0].reader[0].rdrdesc);

   fgets((char *)cfgparam,30,cfg);   /* Reader prox switch? */
   comset[0].reader[0].rdrproc = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader queue size */
   comset[0].reader[0].rdrqsize = atoi((char *) cfgparam);
   qmax[0] = comset[0].reader[0].rdrqsize;

   fgets((char *)cfgparam,30,cfg);   /* Lost reads alarm? */
   comset[0].reader[0].lostreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* No read alarm? */
   comset[0].reader[0].noreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader down alarm? */
   comset[0].reader[0].rdrdownalarm = atoi((char *) cfgparam);

   /* Reader 2 */
   fgets((char *)cfgparam,30,cfg);   /* Reader number */
   comset[0].reader[1].rdrnum = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader location */
   strncpy((char *) comset[0].reader[1].rdrloc, (char *) cfgparam, 10);
   stripnewline((char *) comset[0].reader[1].rdrloc);

   fgets((char *)cfgparam,30,cfg);   /* Reader description */
   strncpy((char *) comset[0].reader[1].rdrdesc, (char *) cfgparam, 35);
   stripnewline(comset[0].reader[1].rdrdesc);

   fgets((char *)cfgparam,30,cfg);   /* Reader prox switch? */
   comset[0].reader[1].rdrproc = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader queue size */
   comset[0].reader[1].rdrqsize = atoi((char *) cfgparam);
   qmax[1] = comset[0].reader[1].rdrqsize;

   fgets((char *)cfgparam,30,cfg);   /* Lost read alarm? */
   comset[0].reader[1].lostreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* No read alarm? */
   comset[0].reader[1].noreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader down alarm? */
   comset[0].reader[1].rdrdownalarm = atoi((char *) cfgparam);

   /* COM2 settings */
   fgets((char *)cfgparam,30,cfg);   /* Device - DL10 or XCI */
   strncpy((char *) comset[1].device, (char *) cfgparam, 10);

   fgets((char *)cfgparam,30,cfg);   /* Baud rate */
   comset[1].baud = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Parity */
   comset[1].parity = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Word length */
   comset[1].wordlen = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Stop bits */
   comset[1].stop = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Controller description */
   strncpy((char *) comset[1].comctrldesc, (char *) cfgparam,5);
   stripnewline((char *) comset[1].comctrldesc);

   fgets((char *)cfgparam,30,cfg);   /* Controller location */
   strncpy((char *) comset[1].ctrlloc, (char *) cfgparam, 5);
   stripnewline((char *) comset[1].ctrlloc);

   fgets((char *)cfgparam,30,cfg);   /* Slow threshold seconds */
   comset[1].slowthresh = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Slow controller alarm? */
   comset[1].slowalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Controller down alarm? */
   comset[1].downalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Status change alarm? */
   comset[1].rdrstatcalarm = atoi((char *) cfgparam);

   /* COM2 Reader setups */
   /* Reader 1 */
   fgets((char *)cfgparam,30,cfg);   /* Reader number */
   comset[1].reader[0].rdrnum = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader location */
   strncpy((char *) comset[1].reader[0].rdrloc, (char *) cfgparam, 10);
   stripnewline((char *) comset[1].reader[0].rdrloc);

   fgets((char *)cfgparam,30,cfg);   /* Reader description */
   strncpy((char *) comset[1].reader[0].rdrdesc, (char *) cfgparam, 35);
   stripnewline(comset[1].reader[0].rdrdesc);

   fgets((char *)cfgparam,30,cfg);   /* Reader prox switch? */
   comset[1].reader[0].rdrproc = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader queue size */
   comset[1].reader[0].rdrqsize = atoi((char *) cfgparam);
   qmax[2] =comset[1].reader[0].rdrqsize;

   fgets((char *)cfgparam,30,cfg);   /* Lost read alarm? */
   comset[1].reader[0].lostreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* No read alarm? */
   comset[1].reader[0].noreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader down alarm */
   comset[1].reader[0].rdrdownalarm = atoi((char *) cfgparam);

   /* Reader 2 */
   fgets((char *)cfgparam,30,cfg);   /* Reader number */
   comset[1].reader[1].rdrnum = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader location */
   strncpy((char *) comset[1].reader[1].rdrloc, (char *) cfgparam, 10);
   stripnewline((char *) comset[1].reader[1].rdrloc);

   fgets((char *)cfgparam,30,cfg);   /* Reader description */
   strncpy((char *) comset[1].reader[1].rdrdesc, (char *) cfgparam, 35);
   stripnewline(comset[1].reader[1].rdrdesc);

   fgets((char *)cfgparam,30,cfg);   /* Reader prox switch? */
   comset[1].reader[1].rdrproc = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader queue size */
   comset[1].reader[1].rdrqsize = atoi((char *) cfgparam);
   qmax[3] = comset[1].reader[1].rdrqsize;

   fgets((char *)cfgparam,30,cfg);   /* Lost read alarm */
   comset[1].reader[1].lostreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* No read alarm */
   comset[1].reader[1].noreadalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Reader down alarm */
   comset[1].reader[1].rdrdownalarm = atoi((char *) cfgparam);

   /*  Program parameters */
   fgets((char *)cfgparam,30,cfg);   /* HLLAPI keystroke delay */
   param.hkeydelay = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* HLLAPI wait time default */
   param.hwait = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast logon wait time default */
   param.logonwait = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast slow time */
   param.bcslow = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast slow alarm? */
   param.bcslowalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast down alarm? */
   param.bcdownalarm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast autologon? */
   param.bcautologon = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Broadcast communications? */
   param.bccomm = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Program password? */
   strncpy((char *) param.password, (char *) cfgparam, 10);
   stripnewline((char *) param.password);

   fgets((char *)cfgparam,30,cfg);   /* Clear counts time - hour */
   param.counthr = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Clear counts time - minute */
   param.countmin = atoi((char *) cfgparam);

   fgets((char *)cfgparam,30,cfg);   /* Machine name */
   strncpy((char *) param.mname, (char *) cfgparam, 10);
   stripnewline((char *) param.mname);

   fgets((char *) cfgparam,30,cfg);  /* DL10 on COM3? */
   param.dl10com3 = atoi((char *) cfgparam);

   fgets((char *) cfgparam,30,cfg);   /* Logical reader for DL10 display */
   param.dl10rdr = atoi((char *) cfgparam);

   sprintf(lastgood[0], "BCPC1100000000NT");  /* Set last good reads array */
   sprintf(lastgood[1], "BCPC3100000000NT");
   sprintf(lastgood[2], "BCPC2100000000NT");
   sprintf(lastgood[3], "BCPC4100000000NT");

   return(0);
}

void drawscreen(void) {
   /*********************************************************************/
   /* PURPOSE: Defines windows and other screen items & displays        */
   /*  PARAMS: none                                                     */
   /* RETURNS: none                                                     */
   /*********************************************************************/

   int c, t, z = 0;            /* General purpose integers */
   char statline[80];          /* Status line buffer */
   csr_hide();                 /* Hide the cursor */
   sw_att(LSYS, FULL_WNP);     /* Change the attribute of the full wn*/
   wn_expset(FULL_WNP);        /* Display a fullscreen window */
   v_chq(177,ENDWN,FULL_WNP);  /* Fast fullscreen  put of character */
   v_chattrow(24,0,32,LDEBUGMSG,ENDROW,CHATT,FULL_WNP);  /* Change statline */
   /* Prepare status line... */
   sprintf(statline," AVI/PC ³                                                                       ");
   v_stattpl(24,0,statline,LDEBUGMSG,STATT,FULL_WNP);  /* Show status line */
	showavgtime();   /* Update status line */
   showsystime();      /* Show system time */

   /* Customize Vermont Views default colors for our own uses... */
   latt_rpl(LSHADOW, NORMAL, LIGHT | BLACK, BLACK, LATT_SYS);
   latt_rpl(LARROW, NORMAL, LIGHT | WHITE, BLACK, LATT_SYS);
   latt_rpl(LDEBUG, NORMAL, LIGHT | BROWN, RED, LATT_SYS);
   latt_rpl(LFLDACT, NORMAL, LIGHT | WHITE, MAGENTA, LATT_SYS);

   for(c=0; c<2; c++) {      /* Check to see which readers installed */
      for(t=0; t<2; t++) {
         if(strlen((char *) comset[c].reader[t].rdrdesc) <2) {
            /* If no reader description, it must not be installed, */
            /* so set the window colors & title */
            rdrwinatt[z] = LSHADOW;
            rdrtitleatt[z] = LSHADOW;
            sprintf((char *) rdrtitle[z],"UNUSED");
         } else {
            /* If there is a desctiption, set colors & window title */
            rdrwinatt[z] = LHIGHLITE;
            rdrtitleatt[z] = LARROW;
            strcpy((char *) rdrtitle[z],(char *) comset[c].reader[t].rdrdesc);
         }
         z++;    /* Advance array subscript */
      }
   }

   for(c=0;c<4;c++) {   /* Draw the reader windows & fields */
      drawrdr(c);
      drawfields(c);
   }

   /* Define error windows for XCI controllers */
   xcierrwin[0] = wn_def(0,0,9,80,LDEBUG,BDR_DLNP);
   xcierrwin[1] = wn_def(9,0,9,80,LDEBUG,BDR_DLNP);

   /* Setup & display message window */
   latt_rpl(LGREEN, NORMAL, BLACK, GREEN, LATT_SYS);
   msgwin = wn_def(18,0,6,80,LGREEN,BDR_DLNP);   /* Define window */
   sw_opt(WORDWRAP, OFF, msgwin);                /* Turn wordwrap off */
   sw_title(msgwintitle,LGREEN,TOPLEFT,msgwin);  /* Show title */
   wn_expset(msgwin);                            /* Explode window up */

   alarmwin = wn_def(18,0,6,80,LGREEN,0);        /* Alarm window define */

   /* Define error window */
   errorwin = wn_def(CENTER_WN, CENTER_WN, 5, 50, LDEBUG, BDR_DLNP);
   sw_shad(TRANSPARENT, LSHADOW, BOTTOMRIGHT, errorwin);
}

void undrawscreen(void) {
   /*********************************************************************/
   /* PURPOSE: Undraws the screen by dropping all windows               */
   /*  PARAMS: none                                                     */
   /* RETURNS: none                                                     */
   /*********************************************************************/

   int c;                      /* Counter */

   wn_expunset(msgwin);        /* Unexplode message window */

   for(c=3;c>-1;c--) {         /* Once for each reader window */
      wn_expunset(rdrwin[c]);  /* Unexplode reader window */
   }

   wn_expunset(FULL_WNP);      /* Unexplode fullscreen window */
   vs_clr();                   /* Clear the screen */
}

void drawrdr(int w) {
   /*********************************************************************/
   /* PURPOSE: Draws a reader window on the screen                      */
   /*  PARAMS: int w - logical reader number to display window for      */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   /* define window */
   rdrwin[w] = wn_def(rdrwiny[w],rdrwinx[w],9,40, rdrwinatt[w], BDR_DLNP);

   /* define window title */
   sw_title(rdrtitle[w],rdrtitleatt[w],TOPLEFT,rdrwin[w]);

   wn_expset(rdrwin[w]);   /* explode window onto screen */
}

void drawfields(int w) {
   /*********************************************************************/
   /* PURPOSE: Draws text items in the reader window                    */
   /*  PARAMS: int w - reader number to display window for              */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   v_stpl(1,1,"TIME: ",rdrwin[w]);
   v_stpl(1,22,"DATE: ",rdrwin[w]);
   v_stpl(3,1,"TAG ID: ",rdrwin[w]);
   v_stpl(3,18,"PORT: ",rdrwin[w]);
   v_stpl(3,29,"TRIP: ",rdrwin[w]);
   v_stpl(4,0,"ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ",rdrwin[w]);
   v_stpl(5,1,"SAME: ",rdrwin[w]);
   v_stpl(5,15,"GOOD: ",rdrwin[w]);
   v_stpl(5,29,"DB: ",rdrwin[w]);
   v_stpl(6,3,"NO:",rdrwin[w]);
   v_stpl(6,15,"GOOD:",rdrwin[w]);
   v_stpl(6,28,"PRX:",rdrwin[w]);
}

void displayxcirecord(int w) {
   /*********************************************************************/
   /* PURPOSE: Displays XCI data in the appropriate reader window       */
   /*  PARAMS: int w - reader number to display                         */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   char nowtime[11], nowdate[9];        /* Date & time holders */

   sw_opt(CLRENDROW, OFF, rdrwin[w]);

   sprintf(nowtime, "%02i:%02i:%02i", xcidata[w].hour, xcidata[w].minute,
      xcidata[w].second);
   sprintf(nowdate, "%02i/%02i/%02i", xcidata[w].month, xcidata[w].day,
      xcidata[w].year);

   v_stattpl(1,8,nowtime,LHELP,STATT,rdrwin[w]);
   v_stattpl(1,28,nowdate,LHELP,STATT,rdrwin[w]);
   v_stattpl(3,9,xcidata[w].tagid,LHELP,STATT,rdrwin[w]);
   v_stattpl(3,24,xcidata[w].readerport,LHELP,STATT,rdrwin[w]);
   v_stattpl(3,35,xcidata[w].tripstatus,LHELP,STATT,rdrwin[w]);
   v_stattpl(5,7,xcidata[w].samereads,LHELP,STATT,rdrwin[w]);
   v_stattpl(5,21,xcidata[w].goodreads,LHELP,STATT,rdrwin[w]);
   v_stattpl(5,33,xcidata[w].dblevel,LHELP,STATT,rdrwin[w]);

   showcounts(w);    /* Show the counts */
}

void showcounts(int w) {
   /*********************************************************************/
   /* PURPOSE: Displays the counts for a specific reader in its window  */
   /*  PARAMS: int w - reader number to display counts for              */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   if (strstr((char *) rdrtitle[w],"UNUSED") == NULL) {
      /* If there's a reader defined for this logical number */
      if(((w == 0 || w == 1) && (!wn_isup(xcierrwin[0]))) ||
         ((w == 2 || w == 3) && (!wn_isup(xcierrwin[1])))) {
         /* If the XCI controller error windows aren't up */
         /* go ahead and show the counts                  */
         sw_opt(CLRENDROW, OFF, rdrwin[w]);
         sprintf(utility, "%05i", rdrbad[w]);
         v_stattpl(6,7,utility,LHELP,STATT,rdrwin[w]);
         sprintf(utility, "%05i", rdrgood[w]);
         v_stattpl(6,21,utility,LHELP,STATT,rdrwin[w]);
         sprintf(utility, "%05i", rdrprox[w]);
         v_stattpl(6,33,utility,LHELP,STATT,rdrwin[w]);
         sw_dim(FULL, rdrwin[w]);

         /* Make sure we aren't dividing by zero */
         if((rdrgood[w] !=0) || ((rdrgood[w]+rdrbad[w]) != 0)) {
            /* Calculate the read rate */
            sprintf(utility, "µ%3.0f%Æ",
               ((float) rdrgood[w]/(rdrgood[w]+rdrbad[w]))*100);
         } else {
            /* If nothing to calculate... */
            sprintf(utility, "µ -- Æ");
         }

         v_stpl(0,33,utility,rdrwin[w]);
         sw_dim(INSIDE, rdrwin[w]);
         sw_opt(CLRENDROW, ON, rdrwin[w]);
      }
   }
}

void showstatus(char * title) {
   /*********************************************************************/
   /* PURPOSE: Shows a popup status window                              */
   /*  PARAMS: char * title - title for status window                   */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   /* Define the status window */
   statuswin = wn_def(CENTER_WN, CENTER_WN, 5, 50, LREVERSE, BDR_DLNP);

   /* Define the status window title */
   sw_title(title, LREVERSE, TOPCENTER, statuswin);

   /* Setup a shadow for the window */
   sw_shad(TRANSPARENT, LSHADOW, BOTTOMRIGHT, statuswin);

   /* Explode it onto the window */
   wn_expset(statuswin);
}

void hidestatus(void) {
   /*********************************************************************/
   /* PURPOSE: Hides the status window                                  */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   wn_expunset(statuswin);    /* Unexplode status window from screen */
}

void showerror(char * title, char * message) {
   /*********************************************************************/
   /* PURPOSE: Pops up an error window with an audible alarm and waits  */
   /*          for the user to hit a key.                               */
   /*  PARAMS: char * title - title of error window                     */
   /*          char * message - error message to display                */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   int t;                          /* Counter */

   for(t=0;t<3;t++) {              /* Three times */
      beep_vv(BPSHORT, BPMIDDLE);  /* Sound middle tone */
      beep_vv(BPSHORT, BPHIGH);    /* Sound high tone */
   }

   sw_title(title, LDEBUGMSG, TOPCENTER, errorwin);   /* Set window title */
   wn_expset(errorwin);            /* Explode error window up */
   v_stpl(1, CENTER_TEXT, message, errorwin);   /* Show error message */
   ki();                           /* Wait for a key */
   wn_expunset(errorwin);          /* Unexplode error window */
}

void changewindow(int win, int att) {
   /*********************************************************************/
   /* PURPOSE: Changes a reader window to a specified attribute         */
   /*  PARAMS: int win - reader window to change                        */
   /*          int att - color attribute to change window to            */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   sw_att(att, rdrwin[win]);      /* Change window attribute */

   sw_bdratt(att, rdrwin[win]);   /* Change border attribute */

   if(((win == 0 || win == 1) &&
      (!wn_isup(xcierrwin[0]))) || ((win == 2 || win == 3)
      && (!wn_isup(xcierrwin[1])))) {

      /* Check to see if XCI error windows are up first */
      wn_dn(rdrwin[win]);         /* Down window */
      wn_up(rdrwin[win]);         /* Up window */
      drawfields(win);            /* Show window items */
      displayxcirecord(win);      /* Show current XCI record for window */
   }
}

void fatalerror(char *errtitle, char * errmsg) {
   /*********************************************************************/
   /* PURPOSE: Shows an error window and then exits the program with    */
   /*          an error condition                                       */
   /*  PARAMS: char * errtitle - title of error window                  */
   /*          char * errmsg - error message to display                 */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   showerror(errtitle,errmsg);    /* Call error window function */
   vs_clr();                      /* Clear the screen */
   exit_vv();                     /* Exit Vermont Views */
   clrscr();                      /* Clear the DOS screen */
   exit(1);                       /* Exit with DOS errorlevel 1 */
}

int strmid(char *string1, int startchar, int length, char *string2) {
   /*********************************************************************/
   /* PURPOSE: Returns a portion of a string in another string          */
   /*  PARAMS: char * string1 - string to retrieve portion from         */
   /*          int startchar - starting position to copy from           */
   /*          int length - length to copy                              */
   /*          char * string2 - string to copy into                     */
   /* RETURNS: always 0                                                 */
   /*********************************************************************/

   int t=startchar,                  /* Starting character */
       endpos = (startchar+length);  /* Ending character offset */

   while(t<endpos) {                 /* While not at endpos */
      string2[(t-startchar)] = string1[t];  /* Copy character by character */
      t++;                           /* Increment offset */
   }
   string2[(t-startchar)] = 0;       /* Insert ASCII 0 to terminate string */
   return(0);                        /* Get outta here! */
}

void outmessage(char * message) {
   /*********************************************************************/
   /* PURPOSE: Outputs a time & date stamped messsage to the message    */
   /*          window                                                   */
   /*  PARAMS: char * message - message to display                      */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct time nowtime;         /* System time structure */
   struct date nowdate;         /* System date structure */
   int day, month, year,        /* Ints for date components */
       hour, minute, second;    /* Ints for time components */

   char msg[255];               /* Buffer for message */

   gettime(&nowtime);           /* Get current time */
   hour = nowtime.ti_hour;      /* Get components */
   minute = nowtime.ti_min;
   second = nowtime.ti_sec;

   getdate(&nowdate);           /* Get current date */
   day = nowdate.da_day;        /* Get components */
   month = nowdate.da_mon;
   year = nowdate.da_year;

   /* Prepare message with date, time, message string */
   sprintf(msg,"%02i/%02i/%02i %02i:%02i:%02i %s\n",
      month,day,(year-initcentury),hour,minute,second,message);

   v_st(msg, msgwin);           /* Output message to message window */
}

void statusmsg(char * msg) {
   /*********************************************************************/
   /* PURPOSE: Shows a message in the status window (showstatus must be */
   /*          called first                                             */
   /*  PARAMS: char * msg - message to display                          */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   wn_clr(statuswin);          /* Clear status window */

   /* Show message in center of window */
   v_stpl(1, CENTER_TEXT, msg, statuswin);
}

int initasync(int com) {
   /*********************************************************************/
   /* PURPOSE: Initializes a com port with an XCI controller attached   */
   /*  PARAMS: int com - comport to initialize (0 or 1)                 */
   /* RETURNS: 0 if successful or 1 if error                            */
   /*********************************************************************/

   /* Show message */
   sprintf(utility, "AVI/PC: Opening COM%i:", (com+1));
   outmessage(utility);

   /* Open comport with parameters from config file */
   comport[com]=a_open((com+1),comset[com].baud,
      comset[com].parity,comset[com].wordlen,
      comset[com].stop,1024,0);    /* 1024 is the comm buffer */

   if (!comport[com]) {            /* If not inited properly */
      /* Prepare and show fatal error message */
      sprintf(utility, "Can't open comm port %i", com);
      fatalerror("ASYNC ERROR",utility);
      return(1);                   /* This code's never reached */
   }

   a_iflush(comport[com]);         /* Clean comm buffer */
   a_putc('r',comport[com]);       /* Put out a status request */

   return(0);
}

int validxci(int rdr) {
   /*********************************************************************/
   /* PURPOSE: Validates an XCI read record                             */
   /*  PARAMS: int rdr - reader to validate record for                  */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   /* Check reader port for numeric only */
   if(validate((char *) xcidata[rdr].readerport,0) != 0) return(1);

   /* Check tagid for numeric only */
   if(validate((char *) xcidata[rdr].tagid,0) != 0) return(1);

   /* Check tripstatus for alpha only */
   if(validate((char *) xcidata[rdr].tripstatus,1) != 0) return(1);

   /* If all OK, we can return a 0 */
   return(0);
}

int xciparse(int com, int rdr) {
   /*********************************************************************/
   /* PURPOSE: Parses an XCI read record into data structure            */
   /*  PARAMS: int com - comport of read record                         */
   /*          int rdr - reader number                                  */
   /* RETURNS: always 0                                                 */
   /*********************************************************************/

   struct time nowtime;        /* System structure for time */
   struct date nowdate;        /* System structure for date */

   getdate(&nowdate);          /* Get current date */

   /* Store date components in xcidata structure */
   xcidata[rdr].day = nowdate.da_day;
   xcidata[rdr].month = nowdate.da_mon;
   xcidata[rdr].year = nowdate.da_year-initcentury;

   gettime(&nowtime);          /* Get current time */

   /* Store time components in xcidata structure */
   xcidata[rdr].hour = nowtime.ti_hour;
   xcidata[rdr].minute = nowtime.ti_min;
   xcidata[rdr].second = nowtime.ti_sec;

   /* Now, parse string from com port into XCI data record structure */

   /* Retrieve ack/nak sequence number */
   strmid(instring[com], 0, 2, xcidata[rdr].acknak);

   /* Get record type */
   strmid(instring[com], 2, 1, xcidata[rdr].status);

   /* Get reader port number */
   strmid(instring[com], 3, 2, xcidata[rdr].readerport);

   /* Get tag id */
   strmid(instring[com], 6, 7, xcidata[rdr].tagid);

   /* Get check digit */
   strmid(instring[com], 13, 1, xcidata[rdr].check);

   /* Get same reads */
   strmid(instring[com], 15, 3, xcidata[rdr].samereads);

   /* Get good reads */
   strmid(instring[com], 19, 3, xcidata[rdr].goodreads);

   /* Get db level */
   strmid(instring[com], 23, 2, xcidata[rdr].dblevel);

   /* Get trip status */
   strmid(instring[com], 25, 1, xcidata[rdr].tripstatus);

   return(0);
}

void showsystime(void) {
   /*********************************************************************/
   /* PURPOSE: Displays current time on te status line                  */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   char nowtime[11];                    /* Buffer for current time string */
   sw_opt(CLRENDROW, OFF, FULL_WNP);    /* Turn clear to end of row off */
   gettimestr(nowtime);                 /* Get current time */
   /* sprintf(utility," %s", nowtime);    Add a space to it */

   /* Show at the lower right hand corner of screen in status line */
   v_stattpl(24,69,nowtime,LDEBUGMSG,STATT,FULL_WNP);

   sw_opt(CLRENDROW, ON, FULL_WNP);     /* Turn clear to end of row on */
}

void showavgtime() {
   /*********************************************************************/
   /* PURPOSE: Displays the status line information                     */
   /*  PARAMS: int sendcount - number of sends to broadcast             */
   /*          int avgtime - average time for broadcast response        */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   char bcstat[] = "3270 OFF";    /* 3270 status message */
   char soundstat[2];             /* Sound status character */

   switch(soundflag) {            /* Determine sound status character */
      case 0:                     /* Sound & flash are on */
         soundstat[0] = 14;       /* Update string with musical note */
         soundstat[1] = '\0';     /* Terminate string */
         break;
      case 1:                     /* Flash is on, sound is off */
         soundstat[0] = 237;      /* Soundflag character is 'í' */
         soundstat[1] = '\0';     /* Terminate string */
         break;
      case 2:                      /* Everything's off */
         sprintf(soundstat," \0"); /* Make string empty */
         break;
      default:
         break;
   }

   sw_opt(CLRENDROW, OFF, FULL_WNP);  /* Turn clear to end of row off */

   /* Determine broadcast comm status message */
   if(param.bccomm == 1) sprintf(bcstat, "3270 ON ");

   /* Show statline 2 different ways, depending on queue status */
   if(!qflag) {   /* Queueing is INACTIVE... */
      sprintf(utility,"COUNT: %06i ³ AVG: %.2f secs ³ %s ³ %-10s ³ %s",
         sendcount, avgtime, bcstat, param.mname,soundstat);
   } else {       /* Queueing is ACTIVE */
      sprintf(utility,"QUEUE: %06i ³ QUEING ACTIVE  ³ %s ³ %-10s ³ %s",
         qcounttotal, bcstat, param.mname,soundstat);
   }

   /* Display status line on the screen */
   v_stattpl(24,10,utility,LDEBUGMSG,STATT,FULL_WNP);
   sw_opt(CLRENDROW, ON, FULL_WNP);   /* Turn clear to end of row on */
}


/***************************************************************************/
/*                             DL10 ROUTINES                               */
/***************************************************************************/
int initdl10(int com) {
   /*********************************************************************/
   /* PURPOSE: Initializes the DL10 display                             */
   /*  PARAMS: int com - comport to initialize DL10 on                  */
   /* RETURNS: 1 if error or 0 if successful                            */
   /*********************************************************************/

   /* Initialize DL10 com port */
   comport[com]=a_open((com+1),9600,PAR_EVEN,7,2,64,0);


   if (!comport[com]) {   /* Error initing? */
      msgalert();

      /* Prepare and show error message */
      sprintf(utility,
         "AVI/PC: ASYNC ERROR! Can't open comm port %i", (com+1));
      outmessage(utility);

      logerror(utility,"E");   /* Log error in ERROR.LOG */
      msgnormal();
      dl10 = -1;               /* Set DL10 flag */
      return(1);               /* Return an error */
   }

   a_iflush(comport[com]);     /* Flush comm buffer */

   cleardl10();                /* Clear the DL10 display */
   showdl10("OK");             /* Display DL10 message */

   /* Show screen message */
   sprintf(utility, "AVI/PC: DL10 initialized on COM%i:",(com+1));
   outmessage(utility);

   return(0);
}

int testdl10() {
   /*********************************************************************/
   /* PURPOSE: Outputs current date & time to DL10 display              */
   /*  PARAMS: none                                                     */
   /* RETURNS: 1 if no DL10 attached or 0 if successful                 */
   /*********************************************************************/

   char nowtime[12],         /* String for current time */
        nowdate[10],         /* String for current date */
        dl10testout[80];     /* DL10 output string */

   if(dl10 == -1) {          /* If there's no DL10 attached */
      outmessage("USER: There's no DL10 attached to test!");
      alarm(1,0);
      return(1);
   }

   getdatestr(nowdate);      /* Get the current date */
   gettimestr(nowtime);      /* Get the current time */

   /* Prepare the string and show on the DL10 display */
   sprintf(dl10testout,"%s %s *", nowdate, nowtime);
   showdl10(dl10testout);

   /* Show user message in message window */
   sprintf(utility, "USER: DL10 test: '%s'", dl10testout);
   outmessage(utility);

   return(0);
}

int cleardl10() {
   /*********************************************************************/
   /* PURPOSE: Clears the DL10 display                                  */
   /*  PARAMS: none                                                     */
   /* RETURNS: 1 if no DL10 attached or 0 if succesful                  */
   /*********************************************************************/

   if(dl10 == -1) return(1);     /* If no DL10 available, return */
   a_putc(27,comport[dl10]);     /* Output ESC character to DL10 */
   a_putc('*',comport[dl10]);    /* Output asterisk to DL10 */
   return(0);
}

int showdl10(char * showstring) {
   /*********************************************************************/
   /* PURPOSE: Displays a message on the DL10 display                   */
   /*  PARAMS: char * showstring - string to display                    */
   /* RETURNS: 1 if no DL10 attached, otherwise 0                       */
   /*********************************************************************/

   if(dl10 == -1) return(1);          /* If no DL10 attached, return error */
   cleardl10();                       /* Clear the DL10 display */
   a_puts(showstring, comport[dl10]); /* Output the string */
   return(0);
}

/***************************************************************************/
/*                          BROADCAST ROUTINES                             */
/***************************************************************************/
int getrdrnum(int portnum) {
   /*********************************************************************/
   /* PURPOSE: Returns a physical reader number for a given port number */
   /*  PARAMS: int portnum - port number                                */
   /* RETURNS: an integer containing the actual reader # (0 or 1)       */
   /*********************************************************************/

   /* If it's a 11 or 21, return 0 */
   if(instring[portnum][3] == '1' || instring[portnum][3] == '2') {
      return(0);
   }

   /* If it's a 31 or 41, return 1 */
   if((instring[portnum][3] == '3') || (instring[portnum][3] == '4')) {
      return(1);
   }

   return(0);
}

int getfieldwin(int portnum) {
   /*********************************************************************/
   /* PURPOSE: Examines a read record from a comport and then (a)       */
   /*          changes the port number and (b) returns a window number  */
   /*          (0 thru 3) to use for the reader window array            */
   /*  PARAMS: int portnum - port number                                */
   /* RETURNS: an integer containg the reader window number (0-3)       */
   /*********************************************************************/

   /* If COM2: and port number is '11' change to '21' */
   if ((portnum == 1) && (instring[portnum][3] == '1')) {
      instring[portnum][3] = '2';
   }

   /* If COM2: and port number is '31' change to '41' */
   if ((portnum == 1) && (instring[portnum][3] == '3')) {
      instring[portnum][3] = '4';
   }

   /* Figure out which reader window number and return it */
   switch(instring[portnum][3]) {
       case '1':
          return(0);
       case '3':
          return(1);
       case '2':
          return(2);
       case '4':
          return(3);
       default:
          return(-1);
        }
        return(0);
}

int getqnumber() {
   /*********************************************************************/
   /* PURPOSE: Examines a read record structure and determines which    */
   /*          logical reader it is from (0 thru 3)                     */
   /*  PARAMS: int portnum - port number                                */
   /* RETURNS: an integer containg the reader window number (0-3)       */
   /*********************************************************************/

   switch(xciqdata.readerport[0]) {
      case '1':
         return(0);
      case '3':
         return(1);
      case '2':
         return(2);
      case '4':
         return(3);
      default:
         return(-1);
        }
        return(0);
}


int xcihandler(int portnum) {
   /*********************************************************************/
   /* PURPOSE: The big 'un - handles messages from the XCI controller   */
   /*          and then calls the appropriate routines                  */
   /*  PARAMS: int portnum - port number                                */
   /* RETURNS: 1 on error or else a 0                                   */
   /*********************************************************************/

   static char * oldack[2][3];     /* Hold old ACK/NAK sequence numbers */
   int winno = -1, rdrno = 0;      /* Ints for reader arrays & reader # */

   /* If comm buffer has more than 4 characters in it, check it out */
   if (a_icount(comport[portnum]) > 4) {
      checkxcistatus(0, portnum);   /* Reset XCI message timer for port */

      /* Get the string from the comm buffer */
      a_gets(instring[portnum], 255, comport[portnum], 90);

      if (isprintstr(instring[portnum]) != 0) {
         /* If the string's garbage... */
	 garbcount[portnum]++;   /* Increment garbage counter */
	 msgalert();

         /* NAK if less than three consecutive garbled strings */
	 if(garbcount[portnum] < 3) {
	    sprintf(utility,"XCI %i: NAKing garbage characters",(portnum+1));
	    outmessage(utility);
	    msgnormal();
	    a_putc(21,comport[portnum]);    /* Send a NAK */
	 } else {
            /* If three consecutive garbled strings... */
	    garbcount[portnum] = 0;  /* Reset garbage counter */
	    sprintf(utility,
               "XCI %i: Three consecutive garbled data recv'd - ACKing",
               (portnum+1));
	    outmessage(utility);
	    msgnormal();
	    a_putc(6,comport[portnum]);    /* Send a ACK */
	 }
         /* Log an error message */
	 sprintf(utility,"XCI %i: Rec'vd garbled data from controller",
            (portnum+1));
         logerror(utility,"O");
         return(1);
      } else {
         /* Good data - reset garbage counter to 0 */
	 garbcount[portnum] = 0;
      }

      /* Display message from XCI port */
      sprintf(utility,"XCI %i: '%s'",(portnum+1), instring[portnum]);
      outmessage(utility);

      /* If XCI error window is up... */
      if(wn_isup(xcierrwin[portnum]) != 0) {
	 wn_expunset(xcierrwin[portnum]);   /* Drop error window */
         errorflag[portnum] = 0;            /* Reset XCI error flag */

         /* Prepare message, display and log... */
         sprintf(utility, "XCI %i: Controller now online", (portnum+1));
         outmessage(utility);
         logerror(utility,"E");
      }

      /* Check what kind of XCI message this is.... */
      switch(instring[portnum][2]) {
         /************************* READ HANDLER ***************************/
         case 'R':   /* Read message */
            if(strlen(instring[portnum]) > 26) {
               /* If more than 26 characters, must be garbage */
               a_putc(21,comport[portnum]);    /* Send a NAK */
               break;                          /* Get out of routine */
            } else {
               winno = getfieldwin(portnum);   /* Change port * get array # */
               rdrno = getrdrnum(portnum);
               xciparse(portnum, winno);       /* Parse the record */
            }

            if((strstr(xcidata[winno].tagid, "???") == NULL)) {
               /* If it's not a no read....*/
               /* Try to validate as XCI record */
               if(validxci(winno) != 0) {
                  /* Show & log error message */
	          sprintf(utility,
                     "XCI %i: Data validation failed - sending NAK",
                     (portnum+1));
	          logerror(utility,"O");
		  outmessage(utility);

		  a_putc(21,comport[portnum]);   /* Send a NAK */
		  break;
	       }

               a_putc(6,comport[portnum]);     /* Send an ACK */

               if(comset[portnum].reader[rdrno].rdrproc == 0) {
                  /* If no prox switch, reset tripstatus to G */
		  sprintf(xcidata[winno].tripstatus,"G");
	       }

               /* If this reader shows on DL10 & ACK/NAK is different */
               if((winno == param.dl10rdr) &&
                  (strcmp((char *) oldack[portnum],
                  xcidata[winno].acknak) != 0)) {

		  if(dl10dbug == 0) {   /* DL10 in normal mode */
		     sprintf(utility, "TAG %s WAIT", xcidata[winno].tagid);
		  } else {              /* DL10 in debug mode */
		     sprintf(utility, "%s %s %s %s*",xcidata[winno].tagid,
                     xcidata[winno].samereads, xcidata[winno].goodreads,
                     xcidata[winno].dblevel);
		  }
		  showdl10(utility);   /* Show DL10 string */
	       }
               logtrans(winno);  /* Log the current read to the audit file */
               if(winno == -1) winno = 1;   /* Why? */
               rdrgood[winno]++;            /* Update good reads count */

               /* Update last good read (in case of no read* */
               sprintf(lastgood[winno], "BCPC%s0%sNT",
                  xcidata[winno].readerport,xcidata[winno].tagid);

               updatecountsfile();   /* Update the counts file */

            } else {
               /* If it's a no read.... */
               logtrans(winno);  /* Log the current read to the audit file */
               rdrbad[winno]++;  /* Increment bad count */
               displayxcirecord(winno);  /* Show the record in the window */
               a_putc(6,comport[portnum]);     /* Send an ACK */

               /* If broadcast comm on and not queing & reader comm on */
               if((param.bccomm == 1)
                  && (qflag == 0) && (rdrbcon[winno] ==1)) {

                  /* Make reader window inverse */
                  changewindow(winno,LHELP);


                  if(sendbcpc(lastgood[winno]) != 0) {
                     /* If error sending noread to broadcast */
                     msgalert();
                     /* Show error message */
                     sprintf(utility, "3270: Error sending no read message!",
                        winno);
                     outmessage(utility);

                     /* Sound alarm if broadcast down alarm on */
                     if(param.bcdownalarm == 1) {
                        alarm(4,1);
                     }
                     logerror(utility,"C");   /* Log the error message */
                  }
                  /* Change window back to normal attribute */
                  changewindow(winno,rdrwinatt[winno]);
               }
               updatecountsfile();   /* Update counts file */
               break;
            }

            /* If ACK/NAK sequence number is different than last time */
            if(strcmp((char *) oldack[portnum],xcidata[winno].acknak) != 0) {
               changewindow(winno,LHELP);   /* Inverse window */

               if((strstr(xcidata[winno].tripstatus, "T") != NULL)) {
                  rdrprox[winno]++;   /* If prox down, increment prox count */
                  updatecountsfile(); /* Update counts file */
               }

               /* Prepare message for broadcast */
               sprintf(bcsend, "BCPC%s0%s%sT",xcidata[winno].readerport,
                  xcidata[winno].tagid,xcidata[winno].tripstatus);


               if((param.bccomm == 1) && (qflag == 0)
                  && (rdrbcon[winno] == 1)) {
                  /* If broadcast is on and not queueing and reader broadcast
                     communications are on */

                  if(sendbcpc(bcsend) != 0) {
                     /* If error sending to broadcast... */
                     msgalert();

                     qflag = 1;      /* Turn queueing flag on */

                     /* Show error message */
                     sprintf(utility,
                        "3270: Broadcast down!  Queuing for reader %i!",
                        winno);
                     outmessage(utility);

                     if(param.bcdownalarm == 1) {
                        /* Sound alarm if parameter set */
                        alarm(4,1);
                     }
                     logerror(utility,"C");   /* Log the error */
                     qhandler(winno);         /* Queue the record */
                  } else {
                     if(winno == param.dl10rdr) {
                        /* If reader displays on DL10 */
                        if(dl10dbug == 0) {   /* DL10 in normal mode */
                           sprintf(utility, "READY");
                        } else {   /* DL10 in debug mode */
                           sprintf(utility, "%s %s %s %s",
                              xcidata[winno].tagid,
                              xcidata[winno].samereads,
                              xcidata[winno].goodreads,
                              xcidata[winno].dblevel);
                        }
                        showdl10(utility);    /* Show DL10 string */
                     }
                  }
               } else {
                  /* If nothing in comm buffer */
                  if(qflag != 0) {  /* If records queued */
                     qhandler(-1);  /* Handle queue with -1 */
                  }
               }
               /* Copy the current ACK/NAK into old ACK/NAK string */
               strcpy((char *) oldack[portnum], xcidata[winno].acknak);
            }
            /* Change the window back to normal */
            changewindow(winno,rdrwinatt[winno]);
            break;
         /********************** STATUS HANDLER ************************/
	 case 'S':        /* Status message */
	    if(strlen(instring[portnum]) > 6) {   /* Longer than 6? */
	       if(strstr(instring[portnum], "SYSTEM TIME OUT") != NULL) {
                  a_putc(6, comport[portnum]);   /* ACK it */

                  /* Show & log error message */
                  sprintf(utility, "XCI %i: Received system time out message",
                     (portnum+1));
                  logerror(utility, "C");
                  msgalert();
                  outmessage(utility);
                  msgnormal();

		  break;
	       } else {
                  /* If not system time out and length > 6, NAK it */
		  a_putc(21, comport[portnum]);
		  break;
	       }
	    } else {
               /* Valid XCI status message */
	       stathandle(portnum);           /* Call status handler */
	       a_putc(6, comport[portnum]);   /* ACK it */
	       break;
	    }
         /*********************** LOST READS HANDLER ***********************/
         case 'L':
            if(strlen(instring[portnum]) > 9) {
               /* If length is larger than 9 it's garbage */
               a_putc(21, comport[portnum]);   /* NAK it */
               break;
            } else {
               a_putc(6,comport[portnum]);    /* ACK it */
               error = losthandler(portnum);  /* Call handler & get losts */

               /* Show & log error message */
               msgalert();
               sprintf(utility, "XCI %i: %i lost reads",(portnum+1),error);
               logerror(utility,"E");
               msgnormal();
            }
	    break;
	 case 'F':      /* FUNCTION mode handler */
	    if(strstr((char *)instring[portnum], "FUNCTION") != NULL) {
               /* If it's a FUNCTION message */
	       a_putc(6, comport[portnum]);   /* ACK it */
	       a_putc(13, comport[portnum]);  /* Send ENTER */
	       a_putc(6, comport[portnum]);   /* ACK it again */
	    } else {
               /* If it's not a FUNCTION message.... */
	       a_putc(6, comport[portnum]);   /* NAK it */
	    }
	    break;
	 /***************************** OTHER ******************************/
	 default:   /* Anything else.... */
            a_putc(6,comport[portnum]);   /* ACK it */
            break;
         }
   } else {
      /* If there's nothing in the comm buffer */
      if(qflag == 1) {      /* If there are records queued */
         /* Call qhandler with -1 to attempt to send records already */
         /* queued to broadcast */
         qhandler(winno);
      }
   }
   checkxcistatus(1, portnum);   /* Start timing for next XCI msg on port */
   checkcounttime(); /* This'll be a general timed events handler */
   return(0);
}

void initstartday() {
   /*********************************************************************/
   /* PURPOSE: Clears the counts at time set in config file             */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date nowdate;            /* System structure for current time */

   getdate(&nowdate);              /* Get the date */
   initday = nowdate.da_day;
   dosbug = nowdate.da_day;
}

void initstartmonth() {
   /*********************************************************************/
   /* PURPOSE: Inits the month number at startup                        */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date nowdate;            /* System structure for current time */

   getdate(&nowdate);              /* Get the date */
   initmonth = nowdate.da_mon;     /* Store it */
}

void initstartyear() {
   /*********************************************************************/
   /* PURPOSE: Initializes year at program startup                      */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date nowdate;            /* System structure for current time */

   getdate(&nowdate);              /* Get the date */

   inityear = nowdate.da_year;    /* Store it */

}


void initstartcentury() {
   /*********************************************************************/
   /* PURPOSE: Initializes century at program startup                   */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   struct date nowdate;            /* System structure for current time */
   int year;
   char nowyear[20],temp[20];

   getdate(&nowdate);              /* Get the date */

   year = nowdate.da_year;         /* Take the year */

   sprintf(temp,"%i", year);       /* Put it in a string */

   strmid(temp,0,2,nowyear);       /* Get the leftmost 2 digits */

   /* convert to integer, multiply by 100 & store as initcentury */
   initcentury = (atoi(nowyear) * 100);

}


int checkcounttime() {
   /*********************************************************************/
   /* PURPOSE: Acts as a general timer routine.  Currently it resets    */
   /*          the counts at a specified time, checks to make sure      */
   /*          DOS advances the date at midnight and corrects it if     */
   /*          not, and starts a new log file at months end.  Relies on */
   /*          values set by initstartmonth, initstartyear, and         */
   /*          initstartcentury to ensure routines are not called more  */
   /*          than necessary.                                          */
   /*  PARAMS: none                                                     */
   /* RETURNS: always a 0                                               */
   /*********************************************************************/

   struct time nowtime;            /* Data structure for current time */
   struct date nowdate;            /* System structure for current time */
   int t, hour, minute,
	    day, month, year;      /* Whaddya think? */
   static int countday;            /* Previous day holder */

   gettime(&nowtime);              /* Get the current time */
   hour = nowtime.ti_hour;         /* Breakout hour */
   minute = nowtime.ti_min;        /* Breakout minute */

   getdate(&nowdate);              /* Get the date */
   day = nowdate.da_day;
   month = nowdate.da_mon;
   year = nowdate.da_year;

   if((hour == param.counthr)
      && (minute == param.countmin) && (day != countday)) {
      /* If it's time to clear the counts */

      countday = day;   /* Store new day */

      /* Show & log message */
      msgalert();
      outmessage("AVI/PC: Performing scheduled delete of counts file");
      alarm(1,0);
      msgnormal();
      logerror("AVI/PC: Performing scheduled delete of counts file","O");

      clearcounts();  /* Clear the counts file */
   }

   if((hour == 0) && (minute == 0)) {
      /* If it's midnight */
      if(dosbug == day) {
         /* Check to see if the day was advanced by comparing it to */
         /* the value stored in dosbug at program startup.  If the  */
         /* same, DOS didn't advance the date properly, so fix it.  */

         /* Show & log the message */
         msgalert();
         outmessage("AVI/PC: Fixing DOS date bug.....");
         logerror("AVI/PC: Fixing DOS date bug.....", "O");
         alarm(1,1);
         msgnormal();

         if(day < monthdays[initmonth]) {    /* If stills days in month */
            day++;                /* Increment day */
         } else {                 /* If now days left in the month */
            day = 1;              /* Day = 1 */
            if(initmonth < 12) {  /* If month less than 12 */
               initmonth++;       /* Add 1 to the month */
            } else {              /* If end of year */
               initmonth = 1;     /* Set month to January */
               inityear++;   /* Add 1 to year (no leap yr. support yet */
	       initstartcentury(); /* Set the century */
            }
         }
         nowdate.da_day = day;        /* Store day in structure */
         nowdate.da_mon = initmonth;  /* Store month in structure */
         nowdate.da_year = inityear;  /* Store year in structure */
         setdate(&nowdate);           /* Set system date */

         /* Show & log message */
         outmessage("AVI/PC: System date corrected....");
         logerror("AVI/PC: System date corrected....","O");
   	}
   }

   /* Check current month with month stored by initstartmonth at */
   /* program startup.                                           */
   if(month != initmonth) {
      /* New month, so open a new audit file */

      /* Show & log message */
      msgalert();
      outmessage("AVI/PC: End of month - opening new log file");
      logerror("AVI/PC: End of month - opening new log file","O");
      msgnormal();

      closelogfile();     /* Close the current log file */
      openlogfile();      /* Open a new one for this month */
      initstartmonth();   /* Initalize the month value */
   }

   if(year != inityear) {
      /* New year - for future use */
      initstartyear();          /* Initialize the new year */
      initstartcentury();       /* Initialize the century */

      /* Display a greeting! */
      msgalert();
      outmessage("AVI/PC: Happy New Year!");
      msgnormal();

   }

   if((hour == 0) && (minute == 1) && (dosbug != day)) {
      /* Reset dosbug at 12:01 AM to prevent calling checkcount twice! */
      initstartday();
   }

   showavgtime();   /* Show the status line */
   return(0);
}

int losthandler(int portnum) {
   /*********************************************************************/
   /* PURPOSE: Handles lost read messages from XCI controller           */
   /*  PARAMS: int portnum - comm port lost reads came in on            */
   /* RETURNS: int containing decimal value of lost count               */
   /*********************************************************************/

   char temp[10],          /* Temporary string */
        port[3];           /* Port string */

   int  rdrport,           /* Reader port */
        lostcount,         /* Decimal lost count */
        rdr = 1,           /* Logical reader */
        rdrnum = 0;        /* Physical reader */

   sprintf(port,"11");     /* Default COM1: reader 1 */

   strmid(instring[portnum],4,1,temp);  /* Get port number from instring */

   rdrport = atoi(temp);   /* Get the   /* Store port in L message */

   /* Assign port values based on port recv'd in L message */

   if((portnum == 0) && (rdrport == 2)) {
      /* If COM1 and lost counts says reader 2... */
      port[0] = 3;    /* So port is '31' */
      rdr = 2;        /* Actual reader number is 2 */
      rdrnum = 1;     /* Logical reader number is 3 */
   }

   if((portnum == 1) && (rdrport == 1)) {
      /* If COM2 and lost counts says reader 1 */
      port[0] = 2;    /* So port is '21' */
      rdr = 1;        /* Actual reader number is 1 */
      rdrnum = 2;     /* Logical reader number is 2 */
   }

   if((portnum == 1) && (rdrport == 2)) {
      /* If COM2 and lost counts says reader 2 */
      port[0] = 4;    /* So it should be '41' */
      rdr = 2;        /* Actual reader number is 2 */
      rdrnum = 3;     /* Logical reader number is 3 */
   }

   strmid(instring[portnum],6,2,temp);  /* Get hex lost count from instring */

   lostcount = hex2dec(temp);    /* Convert it to decimal value */

   /* Prepare lost reads message, show & log */
   sprintf(utility, "XCI %i: %i lost reads at reader %i",
      (portnum+1), lostcount, rdr);
   msgalert();
   outmessage(utility);
   alarm(1,0);
   logerror(utility,"C");
   msgnormal();

   /* Prepare broadcast message */
   sprintf(bcsend, "BCPC%s0%07iLT",port,lostcount);

   if(param.bccomm == 1 && rdrbcon[rdrnum] == 1) {
      /* If broadcast on & reader broadcast is on */
      error = sendbcpc(bcsend);   /* Send to broadcast & get a result code */

      if(error != 0) {   /* If return != 0, and error occured */
         /* Log & show error message */
         sprintf(utility, "3270: Error %i sending lost read count!", error);
         msgalert();
         outmessage(utility);
         logerror(utility, "C");
         msgnormal();
      }
   }
   return(lostcount);   /* Return number of reads lost */
}

int broadcastlogon(void) {
   /*********************************************************************/
   /* PURPOSE: Auto logon to broadcast                                  */
   /*  PARAMS: nothing                                                  */
   /* RETURNS: 0 if successful or 1 on error                            */
   /*********************************************************************/

   char errormsg[40];
   int error;

   showstatus("BROADCAST LOGON");
   gotoxy(1,1);
   csr_hide();
   statusmsg("Connecting to presentation space");

   if ((error = (get3270())) != 0) {
      sprintf(utility, "3270: Error %i connecting to 3270!");
      msgalert();
      outmessage(utility);
      alarm(5,1);
      logerror(utility,"C");
      msgnormal();
      hidestatus();
      return(1);
   }

   if((API_Retc = sendclear()) != 0) {
      sprintf(utility,"3270: Error %i sending clear to broadcast",API_Retc);
      outmessage(utility);
   }

   statusmsg("Requesting ADCICST6....");

   if ((API_Retc = sendkeystrokes("adcicst6",param.hkeydelay)) != 0) {
      msgalert();
      sprintf(utility, "3270: Error %i requesting 'ADCICST6'", API_Retc);
      outmessage(utility);
      alarm(5,1);
      logerror(utility,"C");
      msgnormal();
   }

   statusmsg("Waiting for X to clear....");
   xwait(param.hwait);
   statusmsg("Waiting for CICS/VS logon screen....");

   if (((error = waitforstring("CICS",param.logonwait)))!=0) {
      if(error == -1) {
         msgalert();
         sprintf(utility, "3270: Broadcast failed %i second timeout!",
            param.logonwait);
         outmessage(utility);
         alarm(3,1);
         logerror(utility,"C");
         msgnormal();
      } else {
         msgalert();
         sprintf(errormsg,"3270: HLLAPI returned error %i", error);
         outmessage(utility);
         alarm(3,1);
         logerror(utility,"C");
         msgnormal();
      }
   }
   statusmsg("Sending F12 key....");

   if((error = sendf12()) != 0) {
      msgalert();
      sprintf(utility, "3270: Error %i sending F12 key",error);
      outmessage(utility);
      alarm(3,1);
      logerror(utility,"C");
      msgnormal();
   }

   xwait(param.hwait);

   statusmsg("Sending CLEAR key....");

   if((error = sendclear()) != 0) {
      msgalert();
      sprintf(utility, "3270: Error %i sending CLEAR key",error);
      outmessage(utility);
      alarm(3,1);
      logerror(utility,"C");
      msgnormal();
   }
   hidestatus();
   return(0);
}

int sendbcpc(char * sendmsg) {
   /*********************************************************************/
   /* PURPOSE: Sends a message to broadcast                             */
   /*  PARAMS: char * sendmsg - message to send                         */
   /* RETURNS: 0 if successful or 1 on error                            */
   /*********************************************************************/

   clock_t starttran,       /* Number of clock ticks - start */
           endtran;         /* Number of clock ticks - end */
	float   etime;           /* Elapsed time */
	static  float cumtime;   /* Total time */


   char bcpcresult[30];     /* Buffer for broadcast result messages */

   /* Show message */
   sprintf(utility, "3270: Sending '%s'",sendmsg);
   outmessage(utility);

   starttran = clock();    /* Start timing response time now */

   /* Send the message with key delay setup in config file */
   sendkeystrokes(sendmsg, param.hkeydelay);
   sendcount++;

   /* Show message */
   outmessage("3270: Waiting for broadcast response");

   if(xwait(param.hwait)!=0) {
      /* If waited longer than param setup in config file */

      /* Show error message on screen */
      msgalert();
      sprintf(utility, "3270: Host timed out after %i seconds! ", param.hwait);
      outmessage(utility);

      /* If BC down alarm set & not queueing */
      if((param.bcdownalarm ==1) && (qflag == 0)) alarm(5,1);

      logerror(utility,"C");   /* Log the error */
      return(1);               /* Return an error */
   } else {
      msgnormal();
   }

   endtran = clock();                     /* Set end time for transaction */
   etime = ((endtran - starttran) /18.2); /* Calculate elapsed time */

   cumtime = cumtime + etime;             /* Add elapsed time to cumulative */
   avgtime = (cumtime/sendcount);         /* Calculate average time */

   /* Make HLLAPI call to get string from 3270 session */
   API_Func = 8;
   API_Len = 20;
   API_Retc = 1;
   HLLC(&API_Func,&bcpcresult,&API_Len,&API_Retc);

   if(API_Retc != 0) {
      /* If HLLAPI returned an error */
      sprintf(utility, "3270: Error %i getting BCPC result!", error);
      outmessage(utility);
      logerror(utility,"O");
   } else {
      sprintf(utility,"3270: Received '%s' (%.2f seconds)",&bcpcresult, etime);
      outmessage(utility);
      showavgtime(sendcount, avgtime);
   }

   if(etime >param.bcslow) {
      /* If response from broadcast > value set in config file */
      if(param.bcslowalarm) alarm(3,1); /* If BCSLOW alarm set, alarm! */

      msgalert();
      sprintf(utility,"3270:  Broadcast response time slow (>%i seconds)!",
            param.bcslow);
      outmessage(utility);

      if(param.bcslowalarm == 1) alarm(3,1);

      logerror(utility,"C");
      msgnormal();
   }
   return(0);
}

int checkxcistatus(int flag, int portnum) {
   /*********************************************************************/
   /* PURPOSE: Starts or resets timer of XCI messages & checks status   */
   /*  PARAMS: int flag - 0 to reset timer, 1 check timer               */
   /* RETURNS: 0 if successful or 1 on error                            */
   /*********************************************************************/

   static clock_t last[2],     /* Holder for last time XCI checked */
                  now[2];      /* Holder for now */
   static float   timedif[2];  /* Number of seconds between now & last */
   int t;                      /* Counter */

   if(flag == 0) {
      /* Flag = 0, reset the error flags and XCI timer & exit */
      errorflag[portnum] = 0;
      last[portnum] = clock();
   } else {
      /* Flag = 1, check timer now & do status routine if elapsed time */
      /* is greater than timeout seconds set in config file            */
      now[portnum] = clock();    /* Get the current time */

      /* Calculate the number of seconds between current time and the */
      /* last time a message was received from this comm port         */
      timedif[portnum] = ((now[portnum]-last[portnum]) /CLK_TCK);

      if ((timedif[portnum] > comset[portnum].slowthresh)
         && (errorflag[portnum] != 1)) {
         /* If greater than timeout seconds & error isn't already flagged */
         last[portnum] = clock();   /* Reset the timer */
         errorflag[portnum] = 1;    /* Turn the error flag on */

         /* Show an alert message in the message window */
         msgalert();
         sprintf(utility, "XCI %i: No XCI activity for %i seconds",(portnum+1),
            comset[portnum].slowthresh);
         outmessage(utility);
         msgnormal();

         /* Put a status request out to com port */
         a_putc('r',comport[portnum]);

         /* Clear the comm buffer and wait for a status message */
         a_gets(instring[portnum], 255, comport[portnum], 90);
         a_iflush(comport[portnum]);

         if((instring[portnum][2] != 'S')
            || (strlen(instring[portnum]) <5)) {
            /* If no status message received */
            /* Show down message */
            msgalert();
            sprintf(utility, "XCI %i: Controller down!", (portnum+1));
            outmessage(utility);

            /* Sound alarm if param set */
            if(comset[portnum].downalarm == 1) alarm(5,1);

            logerror(utility,"E");   /* Log the error */
            msgnormal();

            showxcierrwin(portnum);  /* Popup the XCI error window */

            /* Find the first port on XCI with a reader! */
            for(t=0;t<2;t++) {    
               if(comset[portnum].reader[t].rdrnum !=0) {
                  /* Rdr number assigned, so it's a valid reader */
                  error = getportnum(portnum, t);   /* Get port for BCPC */
               }
            }

            /* Prepare broadcast message */
            sprintf(bcsend,"BCPC%i0%.3s %.3sET",
            error,comset[portnum].comctrldesc, comset[portnum].ctrlloc);

            if(param.bccomm == 1) {    /* If broadcast is on */
               sendbcpc(bcsend);       /* Send the message */
            }
            return(1);                 /* Return with error */
         } else {
            /* If status message received */
            msgnormal();
            if(wn_isup(xcierrwin[portnum]) != 0) {
               /* If XCI error windows is now up... */
               wn_expunset(xcierrwin[portnum]);   /* Take it down */

               /* Show & log message */
               sprintf(utility, "XCI %i:  Controller now online", portnum);
               outmessage(utility);
               logerror(utility,"E");

               errorflag[portnum] = 0;   /* Reset the error flag */
            }
            return(0);
         }
      } else {
         if ((timedif[portnum] > comset[portnum].slowthresh)
            && (errorflag[portnum] == 1)) {
            /* If time elapsed and error flag already set */

            last[portnum] = clock();   /* Reset timer */

            /* Prepare and show message */
            sprintf(utility,
               "XCI %i: Inactive for %i seconds; attempting reconnect",
               (portnum+1),comset[portnum].slowthresh);
            msgalert();
            outmessage(utility);
            msgnormal();

            /* Close and reopen com port */
            a_close(comport[portnum],0);
            initasync(portnum);
         }
      }
   }
   return(0);
}


void msgalert(void) {
   /*********************************************************************/
   /* PURPOSE: Changes output in message window to alert color          */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   sw_att(LDEBUG, msgwin);        /* Change interior window attribute */
   sw_bdratt(LDEBUGMSG, msgwin);  /* Change window border attribute */
   wn_upd(msgwin);                /* Update internal VV window structure */
}

void msgnormal(void) {
   /*********************************************************************/
   /* PURPOSE: Changes output in message window to normal color         */
   /*  PARAMS: none                                                     */
   /* RETURNS: nothing                                                  */
   /*********************************************************************/

   sw_att(LGREEN, msgwin);        /* Change interior window attribute */
   sw_bdratt(LGREEN, msgwin);     /* Change window border attribute */
   wn_upd(msgwin);                /* Update internal VV window structure */
}

int openerrlog() {
   /*********************************************************************/
   /* PURPOSE: Opens the ERROR.LOG file                                 */
   /*  PARAMS: none                                                     */
   /* RETURNS: 1 if error or 0 otherwise                                */
   /*********************************************************************/

   char filename[14];              /* Buffer for filename */
   sprintf(filename,"ERROR.LOG");  /* Prepare filename */

   if ((errorfile = fopen(filename, "at")) == NULL) {
      /* If can't open log file */

      /* Show error message */
      msgalert();
      outmessage("AVI/PC: Error opening error log file!");
      msgnormal();

      /* Sound alarm */
      alarm(2,1);

      return(1);      /* Bail out of function with error */
   }
   return(0);
}

int logerror(char * errmsg, char * type) {
   /*********************************************************************/
   /* PURPOSE: Logs a date & time stamped message to the error log      */
   /*  PARAMS: char * errmsg - message to log                           */
   /*          char * type - 1 character error type                     */
   /* RETURNS: 1 if error or 0 otherwise                                */
   /*********************************************************************/

   struct time nowtime;        /* System time structure */
   struct date nowdate;        /* System date structure */

   int day, month, year,       /* Date elements */
       hour, minute, second;   /* Time elements */

   gettime(&nowtime);          /* Get the current time */
   hour = nowtime.ti_hour;     /* Break down into elements */
   minute = nowtime.ti_min;
   second = nowtime.ti_sec;

   getdate(&nowdate);          /* Get the current date */
   day = nowdate.da_day;       /* Break down into elements */
   month = nowdate.da_mon;
   year = nowdate.da_year;

   /* Print the message to the error log file */
   if((error = fprintf(errorfile,"%02i%02i%02i,%02i%02i%02i,%s,%-55s\n",
      (year-initcentury),month,day,hour,minute,second,type,errmsg))==EOF) {
      /* If error writing to log file, show error message */
      outmessage("AVI/PC: Error writing to error log file!");
      return(1);       /* Return with error */
   }

   return(0);          /* Return the result message */
}

int closeerrlog() {
   /*********************************************************************/
   /* PURPOSE: Closes the error log file                                */
   /*  PARAMS: none                                                     */
   /* RETURNS: always 0                                                 */
   /*********************************************************************/

   fclose(errorfile);   /* Close the file */
   return(0);           /* Return */
}

/***************************************************************************/
/*                          BTRIEVE ROUTINES                               */
/***************************************************************************/
int closeqfile() {
   /*********************************************************************/
   /* PURPOSE: Closes the BTRIEVE queue file                            */
   /*  PARAMS: none                                                     */
   /* RETURNS: whatever the BTRV manager returns (0 if OK)              */
   /*********************************************************************/

   int datalen,      /* Size of BTRV data structure */
       bstat,        /* BTRV status */
       bop;          /* BTRV operation code */

   datalen = sizeof(xciqdata);   /* Set data length */

   bop=1;   /* Set BTRV operation code */

   /* Make the BTRV call and get an error code back */
   bstat=BTRV(bop,pos_blk,&xciqdata,&datalen,qkey0,0);

   return(bstat);
}

int openqfile() {
   /*********************************************************************/
   /* PURPOSE: Opens the BTRIEVE queue file                             */
   /*  PARAMS: none                                                     */
   /* RETURNS: whatever the BTRV manager returns (0 if OK)              */
   /*********************************************************************/

   int datalen,      /* Size of BTRV data structure */
       bstat,        /* BTRV status */
       bop;          /* BTRV operation code */

   char filename[14];              /* Buffer for filename */

   datalen = sizeof(xciqdata);

   datalen = sizeof(xciqdata);     /* Set data length */
   sprintf(filename,"XCIQ.BTR");   /* Set the file name */

   bop = 0;                        /* 0 = BTRV open file */

   /* Open the file */
   bstat = BTRV(bop, pos_blk, &xciqdata, &datalen, filename, 0);
   return(bstat);
}

int qhandler(int qnumber) {
   /*********************************************************************/
   /* PURPOSE: Handles queueing of XCI data                             */
   /*  PARAMS: int qnumber - logical reader number or -1 to handle      */
   /*          previously queued reads                                  */
   /* RETURNS: whatever the BTRV manager returns (0 if OK)              */
   /*********************************************************************/

   int datalen,      /* Size of BTRV data structure */
       bstat,        /* BTRV status */
       bop;          /* BTRV operation code */

   int t;            /* Counter */

   for(t=0;t<4;t++) {                     /* Display queue counts */
      sw_opt(CLRENDROW, OFF, rdrwin[t]);
      sw_dim(FULL, rdrwin[t]);
      sprintf(utility, " QUEUE: %06i/%06i ", qcount[t], qmax[t]);
      v_stattpl(8,15,utility,LHELP,STATT,rdrwin[t]);
      sw_dim(INSIDE, rdrwin[t]);
		showavgtime();
   }

   if(qnumber == -1) {
      /* If qnumber is -1, then attempt to send previously  qued records */
      if(param.bccomm == 1) {
         /* If broadcast communications are on */
         qflag = 1;      /* Make sure q flag is set to 1 */
         bop = 12;       /* BTRV get first */

         /* Get the first record from the XCI queue */
         datalen = sizeof(xciqdata);
         bstat=BTRV(bop,pos_blk,&xciqdata,&datalen,qkey0,0);

         if(bstat!=0) {
            /* If error getting first record from queue */
            /* Prepare and show error message */
            msgalert();
            sprintf(utility,
            "QUEUE %i: BTRV error %i getting first rdr rec from queue!",
               t, bstat);
            outmessage(utility);
            msgnormal();

            alarm(4,1);             /* Sound alarm */
            logerror(utility,"O");  /* Log the error */
            return(1);
         }


         t = getqnumber();   /* Get the logical reader number */

         /* Prepare output message */
         sprintf(utility,"QUEUE %i: SEND %s %s %s %s", t,
            xciqdata.date, xciqdata.time, xciqdata.tagid,
            xciqdata.tripstatus);

         /* Show the message */
         outmessage(utility);

         /* Prepare message for broadcast */
         sprintf(bcsend, "BCPC%s0%s%sT",xciqdata.readerport,
            xciqdata.tagid,xciqdata.tripstatus);

         /* OK, we send from here! */
         error = sendbcpc(bcsend);

         if(error == 0) {
            /* If no errors on send to broadcast */

            /* Prepare and show message */
            sprintf(utility,
               "QUEUE %i: Send successful, deleting q record %i of %i",
               t,qcount[t], qmax[t]);
            outmessage(utility);

            bop=4;      /* BTRV delete record */
            datalen = sizeof(xciqdata);
            bstat=BTRV(bop,pos_blk,&xciqdata,&datalen,qkey0,0);

            if(bstat != 0) {
               /* Error deleting record from queue */

               /* Prepare and show error message */
               msgalert();
               sprintf(utility,
                  "QUEUE %i: BTRV error %i deleting first rdr rec from queue!",
                  t, bstat);
               outmessage(utility);
               msgnormal();

               alarm(4,1);             /* Sound alarm */
               logerror(utility,"O");  /* Log the error */
            } else {
               /* If delete successful, update counts */
               qcount[t]--;
               qcounttotal--;
               updatecountsfile();
            }
         }

         if(qcounttotal == 0) {
            /* If queues are empty */
            qflag = 0;      /* Turn off queue flag */

            /* Prepare, show, and log message */
            msgalert();
            outmessage("QUEUE: All broadcast queues now empty");
            logerror("QUEUE: All broadcast queues now empty","O");
            msgnormal();

            alarm(1,0);        /* Sound alarm */

				showavgtime();  /* Update status line */

            for(t=0;t<4;t++) {   /* Redraw the reader windows */
               wn_dn(rdrwin[t]); /* Down window */
               drawrdr(t);       /* Draw reader window */
               drawfields(t);    /* Draw reader window items */
               if (strstr((char *) rdrtitle[t],"UNUSED") == NULL) {
                  /* If reader installed, show counts */
                  showcounts(t);
               }
            }
         }
      }
      return(qcounttotal);   /* Return number of queued reads */
   } else {
     /* Add read to queue for specific reader */
     qcount[qnumber]++;    /* Increment queue counts */
     qcounttotal++;        /* Update queue count totals */
     updatecountsfile();   /* Update the counts file */

     /* Show message */
     sprintf(utility, "QUEUE %i: Adding record to queue", qnumber);
     outmessage(utility);

     /* Add read record to XCI queue file */
     error = addqrecord(qnumber);

     if(error != 0) {
        /* If error adding to queue file */

        /* Prepare, show, and log message */
        sprintf(utility, "QUEUE %i: Error %i adding to queue!",
           qnumber, error);
        msgalert();
        outmessage(utility);
        alarm(3,1);     /* Sound alarm */
        logerror(utility,"O");
        msgnormal();
     }
   }
   return(0);
}

int addqrecord(int qnumber) {   /* Add record to BTRIEVE file */
   /*********************************************************************/
   /* PURPOSE: Adds a record to the XCI queue file                      */
   /*  PARAMS: int qnumber - logical reader number                      */
   /* RETURNS: whatever the BTRV manager returns (0 if OK)              */
   /*********************************************************************/

   char  tempdate[9];

   int datalen,                /* Size of BTRV data structure */
       bstat,                  /* BTRV status */
       bop,                    /* BTRV operation code */
       year, month, day,       /* Integers for date */
       hour, minute, second;   /* Integers for time */

   if(qcount[qnumber] > qmax[qnumber] ) {
      /* If queue count is larger than maximum for this reader.... */

      /* Log & show error message */
      sprintf(utility, "QUEUE %i: Queue full! Discarding first record",
         qnumber);
      outmessage(utility);
      logerror(utility, "Q");

      /* Put readerport into key */
      sprintf(qkey1,"%s",xcidata[qnumber].readerport);
      
      bop = 5;                      /* BTRV Get Equal */
      datalen = sizeof(xciqdata);
      /* Get the first record for this reader from the queue file */
      bstat=BTRV(bop,pos_blk,&xciqdata,&datalen,qkey1,1);

      if(bstat != 0) {
         /* If error getting first */
         msgalert();
         sprintf(utility,
            "QUEUE %i: BTRV error %i getting first rdr rec from queue!",
            qnumber, bstat);
         outmessage(utility);
         alarm(3,1);
         logerror(utility,"Q");
         msgnormal();
      }

      bop=4;                /* BTRV delete record */
      datalen = sizeof(xciqdata);
      /* Delete this record from the queue file */
      bstat=BTRV(bop,pos_blk,&xciqdata,&datalen,qkey1,1);

      if(bstat != 0) {
         /* If error deleting record from the queue file */
         msgalert();
         sprintf(utility, "QUEUE %i: BTRV Error %i deleting first record!",
            qnumber, bstat);
         msgalert();
         outmessage(utility);
         alarm(3,1);
         logerror(utility,"O");
         msgnormal();
      } else {
         /* Deleted record, so decrement counts & update counts file */
         qcount[qnumber]--;
         qcounttotal--;
         updatecountsfile();
      }
   }

   /* Prepare XCI queue record for BTRV */
   sprintf(xciqdata.date, "%02i/%02i/%02i", (xcidata[qnumber].year),
      xcidata[qnumber].month, xcidata[qnumber].day);
   sprintf(xciqdata.time, "%02i:%02i:%02i", xcidata[qnumber].hour,
      xcidata[qnumber].minute, xcidata[qnumber].second);
   strcpy(xciqdata.readerport, xcidata[qnumber].readerport);
   strcpy(xciqdata.tagid, xcidata[qnumber].tagid);
   strcpy(xciqdata.tripstatus, xcidata[qnumber].tripstatus);

   if(qnumber == param.dl10rdr) {
      /* If this reader show on DL10, show broadcast down message */
      sprintf(utility, "TAG %s BC DOWN", xciqdata.tagid);
      showdl10(utility);
   }

   bop = 2;  /* Insert record */
   sprintf(qkey0, "%s", xciqdata.readerport);      /* Copy key to string */
   /* Insert the record into the BTRV XCI queue file */
   bstat = BTRV(bop, pos_blk, &xciqdata, &datalen, qkey0, 0);
   return(bstat);          /* Return the result message */
}

/***************************************************************************/
/***                           THAT IS ALL                               ***/
/***************************************************************************/
