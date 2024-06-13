grammar Logic;
import Cat;

proof: statement* EOF;

statement: letDefinition | inclusion | hypothesis | assertion;

inclusion: FILEINCLUDE FILEPATH;

assertion:
	ASSERT f1 = formula
	| ASSERT e1 = expression INEQUAL e2 = expression; // syntactic sugar

formula:
	p1 = predicate
	| f1 = formula AMP f2 = formula
	| f1 = formula BAR f2 = formula
	| NOT f1 = formula
	| LPAR f1 = formula RPAR;

predicate:
	s1 = expression SEMI s2 = expression; // intersectionNonEmptiness

hypothesis: ASSUME lhs = expression INEQUAL rhs = expression;

FILEINCLUDE: 'include';
ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
FILEPATH: '.'? '/' ~'/' (.)+? '.cat';