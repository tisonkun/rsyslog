/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
struct template {
	struct template *pNext;
	char *pszName;
	int iLenName;
	int tpenElements; /* number of elements in templateEntry list */
	struct templateEntry *pEntryRoot;
	struct templateEntry *pEntryLast;
 	char optFormatForSQL;	/* in text fields,  0 - do not escape,
 	                         * 1 - escape quotes by double quotes,
 				 * 2 - escape "the MySQL way" 
 				 */
	/* following are options. All are 0/1 defined (either on or off).
	 * we use chars because they are faster than bit fields and smaller
	 * than short...
	 */
};

enum EntryTypes { UNDEFINED = 0, CONSTANT = 1, FIELD = 2 };
enum tplFormatTypes { tplFmtDefault = 0, tplFmtMySQLDate = 1,
		      tplFmtRFC3164Date = 2, tplFmtRFC3339Date = 3 };
enum tplFormatCaseConvTypes { tplCaseConvNo = 0, tplCaseConvUpper = 1, tplCaseConvLower = 2 };

/* a specific parse entry */
struct templateEntry {
	struct templateEntry *pNext;
	enum EntryTypes eEntryType;
	union {
		struct {
			char *pConstant;	/* pointer to constant value */
			int iLenConstant;	/* its length */
		} constant;
		struct {
			char *pPropRepl;	/* pointer to property replacer string */
			unsigned iFromPos;	/* for partial strings only chars from this position ... */
			unsigned iToPos;	/* up to that one... */
			enum tplFormatTypes eDateFormat;
			enum tplFormatCaseConvTypes eCaseConv;
			struct { 		/* bit fields! */
				unsigned bEscapeCC: 1;		/* escape control characters? */
				unsigned bDropLastLF: 1;	/* drop last LF char in msg (PIX!) */
			} options;		/* options as bit fields */
		} field;
	} data;
};

struct template* tplConstruct(void);
struct template *tplAddLine(char* pName, char** pRestOfConfLine);
struct template *tplFind(char *pName, int iLenName);
int tplGetEntryCount(struct template *pTpl);
void tplDeleteAll(void);
void tplPrintList(void);

/*
 * vi:set ai:
 */
