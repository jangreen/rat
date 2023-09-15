grammar Logic;
import Cat;

proof: letDefinition* assertion* EOF; // TODO: hypothesis*

assertion: ASSERT f1 = formula;

formula:
	p1 = predicate
	| f1 = formula AMP f2 = formula
	| f1 = formula BAR f2 = formula
	| NOT f1 = formula
	| LPAR f1 = formula RPAR;

predicate:
	s1 = expression SEMI s2 = expression; // intersectionNonEmptiness

/*
 hypothesis: ASSUME lhs = relationExpression INEQUAL rhs = relationExpression; mmAssertion:
 CATASSERT lhs = FILEPATH INEQUAL rhs = FILEPATH
 */

ASSUME: 'assume';
ASSERT: 'assert';
FILEPATH: '.'? '/' ~'/' (.)+? '.cat';
