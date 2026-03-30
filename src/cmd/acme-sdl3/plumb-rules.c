/*
 * Embedded plumber: rule parsing engine.
 * Adapted from src/cmd/plumb/rules.c with renamed symbols.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <thread.h>
#include <ctype.h>
#include <plumb.h>
#include "plumb-internal.h"

typedef struct Input Input;
typedef struct Var Var;

struct Input
{
	char		*file;		/* name of file */
	Biobuf	*fd;		/* input buffer, if from real file */
	uchar	*s;		/* input string, if from /mnt/plumb/rules */
	uchar	*end;	/* end of input string */
	int		lineno;
	Input	*next;	/* file to read after EOF on this one */
};

struct Var
{
	char	*name;
	char	*value;
	char *qvalue;
};

static int		parsing;
static int		nvars;
static Var		*vars;
static Input	*input;

static char 	ebuf[4096];

char *badports[] =
{
	".",
	"..",
	"send",
	nil
};

char *objects[] =
{
	"arg",
	"attr",
	"data",
	"dst",
	"plumb",
	"src",
	"type",
	"wdir",
	nil
};

char *verbs[] =
{
	"add",
	"client",
	"delete",
	"is",
	"isdir",
	"isfile",
	"matches",
	"set",
	"start",
	"to",
	nil
};

static void
printinputstackrev(Input *in)
{
	if(in == nil)
		return;
	printinputstackrev(in->next);
	fprint(2, "%s:%d: ", in->file, in->lineno);
}

void
plumb_printinputstack(void)
{
	printinputstackrev(input);
}

static void
pushinput(char *name, int fd, uchar *str)
{
	Input *in;
	int depth;

	depth = 0;
	for(in=input; in; in=in->next)
		if(depth++ >= 10)
			plumb_parseerror("include stack too deep; max 10");

	in = plumb_emalloc(sizeof(Input));
	in->file = plumb_estrdup(name);
	in->next = input;
	input = in;
	if(str)
		in->s = str;
	else{
		in->fd = plumb_emalloc(sizeof(Biobuf));
		if(Binit(in->fd, fd, OREAD) < 0)
			plumb_parseerror("can't initialize Bio for rules file: %r");
	}

}

int
plumb_popinput(void)
{
	Input *in;

	in = input;
	if(in == nil)
		return 0;
	input = in->next;
	if(in->fd){
		Bterm(in->fd);
		free(in->fd);
	}
	free(in);
	return 1;
}

static int
pgetc(void)
{
	if(input == nil)
		return Beof;
	if(input->fd)
		return Bgetc(input->fd);
	if(input->s < input->end)
		return *(input->s)++;
	return -1;
}

static char*
pgetline(void)
{
	static int n = 0;
	static char *s;
	int c, i;

	i = 0;
	for(;;){
		c = pgetc();
		if(c < 0)
			return nil;
		if(i == n){
			n += 100;
			s = plumb_erealloc(s, n);
		}
		if(c<0 || c=='\0' || c=='\n')
			break;
		s[i++] = c;
	}
	s[i] = '\0';
	return s;
}

static int
lookup(char *s, char *tab[])
{
	int i;

	for(i=0; tab[i]!=nil; i++)
		if(strcmp(s, tab[i])==0)
			return i;
	return -1;
}

Var*
lookupvariable(char *s, int n)
{
	int i;

	for(i=0; i<nvars; i++)
		if(n==strlen(vars[i].name) && memcmp(s, vars[i].name, n)==0)
			return vars+i;
	return nil;
}

char*
variable(char *s, int n)
{
	Var *var;

	var = lookupvariable(s, n);
	if(var)
		return var->qvalue;
	return nil;
}

void
setvariable(char  *s, int n, char *val, char *qval)
{
	Var *var;

	var = lookupvariable(s, n);
	if(var){
		free(var->value);
		free(var->qvalue);
	}else{
		vars = plumb_erealloc(vars, (nvars+1)*sizeof(Var));
		var = vars+nvars++;
		var->name = plumb_emalloc(n+1);
		memmove(var->name, s, n);
	}
	var->value = plumb_estrdup(val);
	var->qvalue = plumb_estrdup(qval);
}

static char*
scanvarname(char *s)
{
	if(isalpha((uchar)*s) || *s=='_')
		do
			s++;
		while(isalnum((uchar)*s) || *s=='_');
	return s;
}

static char*
nonnil(char *s)
{
	if(s == nil)
		return "";
	return s;
}

static char*
filename(Exec *e, char *name)
{
	static char *buf;

	free(buf);
	if(name!=nil && name[0]!='\0'){
		buf = plumb_estrdup(name);
		return cleanname(buf);
	}
	if(e->msg->data[0]=='/' || e->msg->wdir==nil || e->msg->wdir[0]=='\0'){
		buf = plumb_estrdup(e->msg->data);
		return cleanname(buf);
	}
	buf = plumb_emalloc(strlen(e->msg->wdir)+1+strlen(e->msg->data)+1);
	sprint(buf, "%s/%s", e->msg->wdir, e->msg->data);
	return cleanname(buf);
}

char*
dollar(Exec *e, char *s, int *namelen)
{
	int n;
	ulong m;
	char *t;
	static char *abuf;

	if(e!=nil && '0'<=s[0] && s[0]<='9'){
		m = strtoul(s, &t, 10);
		*namelen = t-s;
		if(t==s || m>=NMATCHSUBEXP)
			return "";
		return nonnil(e->match[m]);
	}

	n = scanvarname(s)-s;
	*namelen = n;
	if(n == 0)
		return nil;

	if(e != nil){
		if(n == 3){
			if(memcmp(s, "src", 3) == 0)
				return nonnil(e->msg->src);
			if(memcmp(s, "dst", 3) == 0)
				return nonnil(e->msg->dst);
			if(memcmp(s, "dir", 3) == 0)
				return filename(e, e->dir);
		}
		if(n == 4){
			if(memcmp(s, "attr", 4) == 0){
				free(abuf);
				abuf = plumbpackattr(e->msg->attr);
				return nonnil(abuf);
			}
			if(memcmp(s, "data", 4) == 0)
				return nonnil(e->msg->data);
			if(memcmp(s, "file", 4) == 0)
				return filename(e, e->file);
			if(memcmp(s, "type", 4) == 0)
				return nonnil(e->msg->type);
			if(memcmp(s, "wdir", 3) == 0)
				return nonnil(e->msg->wdir);
		}
	}

	return variable(s, n);
}

static void
ruleerror(char *msg)
{
	if(parsing){
		parsing = 0;
		plumb_parseerror("%s", msg);
	}
	plumb_error("%s", msg);
}

/* expand one blank-terminated string, processing quotes and $ signs */
char*
plumb_expand(Exec *e, char *s, char **ends)
{
	char *p, *ep, *val;
	int namelen, vallen, quoting, inputleft;

	p = ebuf;
	ep = ebuf+sizeof ebuf-1;
	quoting = 0;
	for(;;){
		inputleft = (*s!='\0' && (quoting || (*s!=' ' && *s!='\t')));
		if(!inputleft || p==ep)
			break;
		if(*s == '\''){
			s++;
			if(!quoting)
				quoting = 1;
			else if(*s == '\''){
				*p++ = '\'';
				s++;
			}else
				quoting = 0;
			continue;
		}
		if(quoting || *s!='$'){
			*p++ = *s++;
			continue;
		}
		s++;
		val = dollar(e, s, &namelen);
		if(val == nil){
			*p++ = '$';
			continue;
		}
		vallen = strlen(val);
		if(ep-p < vallen)
			break;
		strcpy(p, val);
		p += vallen;
		s += namelen;
	}
	if(inputleft)
		ruleerror("expanded string too long");
	else if(quoting)
		ruleerror("runaway quoted string literal");
	if(ends)
		*ends = s;
	*p = '\0';
	return ebuf;
}

void
regerror(char *msg)
{
	ruleerror(msg);
}

void
parserule(Rule *r)
{
	r->qarg = plumb_estrdup(plumb_expand(nil, r->arg, nil));
	switch(r->obj){
	case OArg:
	case OAttr:
	case OData:
	case ODst:
	case OType:
	case OWdir:
	case OSrc:
		if(r->verb==VClient || r->verb==VStart || r->verb==VTo)
			plumb_parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		if(r->obj!=OAttr && (r->verb==VAdd || r->verb==VDelete))
			plumb_parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		if(r->verb == VMatches){
			r->regex = regcomp(r->qarg);
			return;
		}
		break;
	case OPlumb:
		if(r->verb!=VClient && r->verb!=VStart && r->verb!=VTo)
			plumb_parseerror("%s not valid verb for object %s", verbs[r->verb], objects[r->obj]);
		break;
	}
}

int
assignment(char *p)
{
	char *var, *qval;
	int n;

	var = p;
	p = scanvarname(p);
	n = p-var;
	if(n == 0)
		return 0;
	while(*p==' ' || *p=='\t')
		p++;
	if(*p++ != '=')
		return 0;
	while(*p==' ' || *p=='\t')
		p++;
	qval = plumb_expand(nil, p, nil);
	setvariable(var, n, p, qval);
	return 1;
}

int
include(char *s)
{
	char *t, *args[3], buf[128];
	int n, fd;

	if(strncmp(s, "include", 7) != 0)
		return 0;
	n = tokenize(s, args, nelem(args));
	if(n < 2)
		goto Err;
	if(strcmp(args[0], "include") != 0)
		goto Err;
	if(args[1][0] == '#')
		goto Err;
	if(n>2 && args[2][0] != '#')
		goto Err;
	t = args[1];
	fd = open(t, OREAD);
	if(fd<0 && t[0]!='/' && strncmp(t, "./", 2)!=0 && strncmp(t, "../", 3)!=0){
		snprint(buf, sizeof buf, "#9/plumb/%s", t);
		t = unsharp(buf);
		fd = open(t, OREAD);
	}
	if(fd < 0)
		plumb_parseerror("can't open %s for inclusion", t);
	pushinput(t, fd, nil);
	return 1;

    Err:
	plumb_parseerror("malformed include statement");
	return 0;
}

Rule*
readrule(int *eof)
{
	Rule *rp;
	char *line, *p;
	char *word;

Top:
	line = pgetline();
	if(line == nil){
		if((input!=nil && input->end==nil) && plumb_popinput())
			goto Top;
		*eof = 1;
		return nil;
	}
	input->lineno++;

	for(p=line; *p==' ' || *p=='\t'; p++)
		;
	if(*p=='\0' || *p=='#')
		return nil;

	if(include(p))
		goto Top;

	if(assignment(p))
		return nil;

	rp = plumb_emalloc(sizeof(Rule));

	/* object */
	for(word=p; *p!=' ' && *p!='\t'; p++)
		if(*p == '\0')
			plumb_parseerror("malformed rule");
	*p++ = '\0';
	rp->obj = lookup(word, objects);
	if(rp->obj < 0){
		if(strcmp(word, "kind") == 0)
			rp->obj = OType;
		else
			plumb_parseerror("unknown object %s", word);
	}

	/* verb */
	while(*p==' ' || *p=='\t')
		p++;
	for(word=p; *p!=' ' && *p!='\t'; p++)
		if(*p == '\0')
			plumb_parseerror("malformed rule");
	*p++ = '\0';
	rp->verb = lookup(word, verbs);
	if(rp->verb < 0)
		plumb_parseerror("unknown verb %s", word);

	/* argument */
	while(*p==' ' || *p=='\t')
		p++;
	if(*p == '\0')
		plumb_parseerror("malformed rule");
	rp->arg = plumb_estrdup(p);

	parserule(rp);

	return rp;
}

void
freerule(Rule *r)
{
	free(r->arg);
	free(r->qarg);
	free(r->regex);
}

void
freerules(Rule **r)
{
	while(*r)
		freerule(*r++);
}

void
freeruleset(Ruleset *rs)
{
	freerules(rs->pat);
	free(rs->pat);
	freerules(rs->act);
	free(rs->act);
	free(rs->port);
	free(rs);
}

Ruleset*
readruleset(void)
{
	Ruleset *rs;
	Rule *r;
	int eof, inrule, i, ncmd;
	char *plan9root;

	plan9root = get9root();
	if(plan9root)
		setvariable("plan9", 5, plan9root, plan9root);

   Again:
	eof = 0;
	rs = plumb_emalloc(sizeof(Ruleset));
	rs->pat = plumb_emalloc(sizeof(Rule*));
	rs->act = plumb_emalloc(sizeof(Rule*));
	inrule = 0;
	ncmd = 0;
	for(;;){
		r = readrule(&eof);
		if(eof)
			break;
		if(r==nil){
			if(inrule)
				break;
			continue;
		}
		inrule = 1;
		switch(r->obj){
		case OArg:
		case OAttr:
		case OData:
		case ODst:
		case OType:
		case OWdir:
		case OSrc:
			rs->npat++;
			rs->pat = plumb_erealloc(rs->pat, (rs->npat+1)*sizeof(Rule*));
			rs->pat[rs->npat-1] = r;
			rs->pat[rs->npat] = nil;
			break;
		case OPlumb:
			rs->nact++;
			rs->act = plumb_erealloc(rs->act, (rs->nact+1)*sizeof(Rule*));
			rs->act[rs->nact-1] = r;
			rs->act[rs->nact] = nil;
			if(r->verb == VTo){
				if(rs->npat>0 && rs->port != nil)
					plumb_parseerror("too many ports");
				if(lookup(r->qarg, badports) >= 0)
					plumb_parseerror("illegal port name %s", r->qarg);
				rs->port = plumb_estrdup(r->qarg);
			}else
				ncmd++;
			break;
		}
	}
	if(ncmd > 1){
		freeruleset(rs);
		plumb_parseerror("ruleset has more than one client or start action");
	}
	if(rs->npat>0 && rs->nact>0)
		return rs;
	if(rs->npat==0 && rs->nact==0){
		freeruleset(rs);
		return nil;
	}
	if(rs->nact==0 || rs->port==nil){
		freeruleset(rs);
		plumb_parseerror("ruleset must have patterns and actions");
		return nil;
	}

	/* declare ports */
	for(i=0; i<rs->nact; i++)
		if(rs->act[i]->verb != VTo){
			freeruleset(rs);
			plumb_parseerror("ruleset must have actions");
			return nil;
		}
	for(i=0; i<rs->nact; i++)
		plumb_addport(rs->act[i]->qarg);
	freeruleset(rs);
	goto Again;
}

Ruleset**
plumb_readrules(char *name, int fd)
{
	Ruleset *rs, **rules;
	int n;

	parsing = 1;
	pushinput(name, fd, nil);
	rules = plumb_emalloc(sizeof(Ruleset*));
	for(n=0; (rs=readruleset())!=nil; n++){
		rules = plumb_erealloc(rules, (n+2)*sizeof(Ruleset*));
		rules[n] = rs;
		rules[n+1] = nil;
	}
	plumb_popinput();
	parsing = 0;
	return rules;
}
