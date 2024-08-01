grammar Logic;
import Cat;

proof: statement* EOF;

statement: letDefinition | inclusion | hypothesis | assertion;

inclusion: FILEINCLUDE FILEPATH;

assertion: ASSERT e1 = expression INEQUAL e2 = expression;

hypothesis: ASSUME lhs = expression INEQUAL rhs = expression;

FILEINCLUDE: 'include';
ASSUME: 'assume';
ASSERT: 'assert';
INEQUAL: '<=';
FILEPATH: '.'? '/' ~'/' (.)+? ('.cat' | '.kat');