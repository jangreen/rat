grammar Cat;

@header {
}

mcm: /*(NAME)?*/ definition+ EOF;

definition: axiomDefinition | letDefinition | letRecDefinition;

axiomDefinition:
	(flag = FLAG)? (negate = NOT)? ACYCLIC e = relationExpression (
		AS RELNAME
	)?
	| (flag = FLAG)? (negate = NOT)? IRREFLEXIVE e = relationExpression (
		AS RELNAME
	)?
	| (flag = FLAG)? (negate = NOT)? EMPTY e = relationExpression (
		AS RELNAME
	)?;

letDefinition: LET n = RELNAME EQ e = relationExpression;

letRecDefinition:
	LET REC n = RELNAME EQ e = relationExpression letRecAndDefinition*;

letRecAndDefinition: AND n = RELNAME EQ e = relationExpression;

/* set and relation expressions are defined in one recursive definiton since antlr does not support mutual left-recursion (only direct left-recursion) */
setExpression:
	e1 = setExpression BAR e2 = setExpression		# setUnion
	| e1 = setExpression AMP e2 = setExpression		# setIntersection
	| n = SETNAME									# setBasic
	| LPAR e1 = setExpression RPAR					# set
	| s = setExpression SEMI r = relationExpression	# setImage
	| r = relationExpression SEMI s = setExpression	# setDomain
	| LCBRAC l = SETNAME RCBRAC						# singleton;

relationExpression:
	e1 = setExpression STAR e2 = setExpression					# cartesianProduct
	| e = relationExpression (POW)? STAR						# transReflexiveClosure
	| e = relationExpression (POW)? PLUS						# transitiveClosure
	| e = relationExpression (POW)? INV							# relationInverse
	| e = relationExpression OPT								# relationOptional
	| NOT e = relationExpression								# relationComplement
	| e1 = relationExpression SEMI e2 = relationExpression		# relationComposition
	| e1 = relationExpression BAR e2 = relationExpression		# relationUnion
	| e1 = relationExpression BSLASH e2 = relationExpression	# relationMinus
	| e1 = relationExpression AMP e2 = relationExpression		# relationIntersection
	| LBRAC DOMAIN_ LPAR e = relationExpression RPAR RBRAC		# relationDomainIdentity
	| LBRAC RANGE LPAR e = relationExpression RPAR RBRAC		# relationRangeIdentity
	| (
		TOID LPAR e = setExpression RPAR
		| LBRAC e = setExpression RBRAC
	)									# relationIdentity
	| FENCEREL LPAR n = NAME RPAR		# relationFencerel
	| LPAR e = relationExpression RPAR	# relation
	| n = NAME							# relationBasic;

LET: 'let';
REC: 'rec';
AND: 'and';
AS: 'as';
TOID: 'toid';

ACYCLIC: 'acyclic';
IRREFLEXIVE: 'irreflexive';
EMPTY: 'empty';

EQ: '=';
STAR: '*';
PLUS: '+';
OPT: '?';
INV: '-1';
NOT: '~';
AMP: '&';
BAR: '|';
SEMI: ';';
BSLASH: '\\';
POW: ('^');

LPAR: '(';
RPAR: ')';
LBRAC: '[';
RBRAC: ']';
LCBRAC: '{';
RCBRAC: '}';

FENCEREL: 'fencerel';
DOMAIN_: 'domain';
// https: //stackoverflow.com/questions/31751723/math-h-macro-collisions
RANGE: 'range';

FLAG: 'flag';

SETNAME: [A-Z0-9\-_.]+;
RELNAME: [a-z\-_.]+;

LINE_COMMENT: '//' ~[\n]* -> skip;

BLOCK_COMMENT: '(*' (.)*? '*)' -> skip;

WS: [ \t\r\n]+ -> skip;

INCLUDE: 'include "' .*? '"' -> skip;

MODELNAME: '"' .*? '"' -> skip;