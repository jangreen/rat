grammar Cat;

@header {
}

mcm: (NAME)? definition+ EOF;

definition: axiomDefinition | letDefinition | letRecDefinition;

axiomDefinition:
	(flag = FLAG)? (negate = NOT)? ACYCLIC e = relationExpression (
		AS NAME
	)?
	| (flag = FLAG)? (negate = NOT)? IRREFLEXIVE e = relationExpression (
		AS NAME
	)?
	| (flag = FLAG)? (negate = NOT)? EMPTY e = relationExpression (
		AS NAME
	)?;

letDefinition: LET n = NAME EQ e = relationExpression;

letRecDefinition:
	LET REC n = NAME EQ e = relationExpression letRecAndDefinition*;

letRecAndDefinition: AND n = NAME EQ e = relationExpression;

setExpression:
	e1 = setExpression BAR e2 = setExpression	# setUnion
	| e1 = setExpression AMP e2 = setExpression	# setIntersection
	| n = NAME									# setBasic
	| LCBRAC l = NAME RCBRAC					# singleton;

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

NAME: [A-Za-z0-9\-_.]+;

LINE_COMMENT: '//' ~[\n]* -> skip;

BLOCK_COMMENT: '(*' (.)*? '*)' -> skip;

WS: [ \t\r\n]+ -> skip;

INCLUDE: 'include "' .*? '"' -> skip;

MODELNAME: '"' .*? '"' -> skip;