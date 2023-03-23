grammar Logic;
import Cat;

statement: hypothesis* (assertion | mmAssertion) EOF;

hypothesis: ASSUME lhs = expression INEQUAL rhs = expression;
assertion: ASSERT lhs = expression INEQUAL rhs = expression;
mmAssertion: CATASSERT lhs = NAME INEQUAL rhs = NAME;

ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
CATASSERT: 'cat assert';
