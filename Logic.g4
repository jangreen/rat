grammar Logic;
import Cat;

statement:
	letDefinition* hypothesis* (assertion | mmAssertion)* EOF;

hypothesis: ASSUME lhs = expression INEQUAL rhs = expression;
assertion: ASSERT lhs = expression INEQUAL rhs = expression;
mmAssertion: CATASSERT lhs = FILEPATH INEQUAL rhs = FILEPATH;

ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
CATASSERT: 'cat assert';
FILEPATH: '.'? '/' ~'/' (.)+? '.cat';
