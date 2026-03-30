/*
 * Embedded plumber internals.
 * Adapted from src/cmd/plumb/plumber.h with renamed symbols
 * to avoid conflicts with acme's own symbols:
 *   expand    -> plumb_expand    (acme has expand() for text expansion)
 *   emalloc   -> plumb_emalloc   (acme's takes uint, plumber's takes long)
 *   erealloc  -> plumb_erealloc  (same)
 *   estrdup   -> plumb_estrdup   (same)
 *   error     -> plumb_error     (different signatures)
 */

typedef struct Exec Exec;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;

/*
 * Object
 */
enum
{
	OArg,
	OAttr,
	OData,
	ODst,
	OPlumb,
	OSrc,
	OType,
	OWdir
};

/*
 * Verbs
 */
enum
{
	VAdd,	/* apply to OAttr only */
	VClient,
	VDelete,	/* apply to OAttr only */
	VIs,
	VIsdir,
	VIsfile,
	VMatches,
	VSet,
	VStart,
	VTo
};

struct Rule
{
	int	obj;
	int	verb;
	char	*arg;		/* unparsed string of all arguments */
	char	*qarg;	/* quote-processed arg string */
	Reprog	*regex;
};

struct Ruleset
{
	int	npat;
	int	nact;
	Rule	**pat;
	Rule	**act;
	char	*port;
};

enum
{
	NMATCHSUBEXP = 100
};

struct Exec
{
	Plumbmsg	*msg;
	char			*match[NMATCHSUBEXP];
	int			p0;		/* begin and end of match */
	int			p1;
	int			clearclick;	/* click was expanded; remove attribute */
	int			setdata;	/* data should be set to $0 */
	int			holdforclient;	/* exec'ing client; keep message until port is opened */
	/* values of $variables */
	char			*file;
	char 			*dir;
};

void		plumb_parseerror(char*, ...);
void		plumb_error(char*, ...);
void*	plumb_emalloc(long);
void*	plumb_erealloc(void*, long);
char*	plumb_estrdup(char*);
Ruleset**	plumb_readrules(char*, int);
Exec*	plumb_matchruleset(Plumbmsg*, Ruleset*);
void		plumb_freeexec(Exec*);
char*	plumb_startup(Ruleset*, Exec*);
char*	plumb_expand(Exec*, char*, char**);
void		plumb_makeports(Ruleset*[]);
void		plumb_addport(char*);
void		plumb_printinputstack(void);
int		plumb_popinput(void);

extern Ruleset	**plumb_rules;
extern char	*plumb_user;
extern char	*plumb_home;
extern jmp_buf	plumb_parsejmp;
extern char	*plumb_lasterror;
extern char	**plumb_ports;
extern int	plumb_nports;
extern int	plumb_debug;

int	plumb_init(void);
int	plumb_send(Plumbmsg*);
