grammar Cat;

@header {}

mcm: definition+ EOF;

definition: axiomDefinition | letDefinition | letRecDefinition;

axiomDefinition:
	(flag = FLAG)? (negate = NOT)? ACYCLIC e = expression (
		AS RELNAME
	)?
	| (flag = FLAG)? (negate = NOT)? IRREFLEXIVE e = expression (
		AS RELNAME
	)?
	| (flag = FLAG)? (negate = NOT)? EMPTY e = expression (
		AS RELNAME
	)?;

letDefinition:
	LET n = RELNAME EQ e = expression
	| LET n = SETNAME EQ e = expression;

letRecDefinition:
	LET REC n = RELNAME EQ e = expression letRecAndDefinition*;

letRecAndDefinition: AND n = RELNAME EQ e = expression;

/*
 set and relation expressions are defined in one recursive definiton since antlr does not support
 mutual left-recursion (only direct left-recursion) -> have to check type of expression while
 parsing!
 */
// operator precdence is determined by the ordering
expression:
	e1 = expression STAR e2 = expression	# cartesianProduct
	| e = expression (POW)? STAR			# transitiveReflexiveClosure
	| e = expression (POW)? PLUS			# transitiveClosure
	| e = expression (POW)? INV				# relationInverse
	| e = expression OPT					# relationOptional
	//| NOT e = expression							# relationComplement
	| e1 = expression SEMI e2 = expression			# composition // domain, image, relational composition
	| e1 = expression AMP e2 = expression			# intersection // set intersection, relational intersection
	| e1 = expression BSLASH e2 = expression		# relationMinus
	| e1 = expression BAR e2 = expression			# union // set union, relational union
	| LBRAC DOMAIN_ LPAR e = expression RPAR RBRAC	# relationDomainIdentity
	| LBRAC RANGE LPAR e = expression RPAR RBRAC	# relationRangeIdentity
	| (
		TOID LPAR e = expression RPAR
		| LBRAC e = expression RBRAC
	)									# relationIdentity
	| FENCEREL LPAR n = RELNAME RPAR	# relationFencerel
	| LPAR e1 = expression RPAR			# parentheses
	| EMPTYSET							# emptyset
	| n = RELNAME						# relationBasic
	| n = SETNAME						# setBasic
	| LCBRAC l = SETNAME RCBRAC			# setSingleton;

LET: 'let';
REC: 'rec';
AND: 'and';
AS: 'as';
TOID: 'toid';

ACYCLIC: 'acyclic';
IRREFLEXIVE: 'irreflexive';
EMPTY: 'empty';

EMPTYSET: '0';
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

RELNAME: [a-z0-9\-_.]+;
SETNAME: [A-Z] [a-zA-Z0-9\-_.]*;

LINE_COMMENT: '//' ~[\n]* -> skip;

BLOCK_COMMENT: '(*' (.)*? '*)' -> skip;

WS: [ \t\r\n]+ -> skip;

INCLUDE: 'include "' .*? '"' -> skip;

MODELNAME: '"' .*? '"' -> skip;