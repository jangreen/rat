grammar Logic;
import Cat;

statement: hypothesis* assertion+ EOF;

hypothesis: ASSUME lhs = expression INEQUAL rhs = expression;
assertion: ASSERT lhs = expression INEQUAL rhs = expression;

ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
