grammar Logic;
import Cat;

statement:
	letDefinition* hypothesis* (assertion | mmAssertion)* EOF;

hypothesis:
	ASSUME lhs = relationExpression INEQUAL rhs = relationExpression;
assertion:
	ASSERT lhs = relationExpression INEQUAL rhs = relationExpression;
mmAssertion: CATASSERT lhs = FILEPATH INEQUAL rhs = FILEPATH;

ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
CATASSERT: 'cat assert';
FILEPATH: '.'? '/' ~'/' (.)+? '.cat';
