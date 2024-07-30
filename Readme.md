# RAT: Relational Algebra (with) Tableaus

RAT is a tool that automatically compares weak memory models formalised in CAT.

## Requirements

- [Antlr](https://www.antlr.org)
- [Java](https://www.java.com/de/download/manual.jsp)
- [Graphviz](https://graphviz.org/)
- [Cmake](https://cmake.org/download/)
- [Boost](https://www.boost.org/users/download/)
- [Spdlog](https://github.com/gabime/spdlog)

## Installation

### Docker

The docker image has all dependencies pre-installed to run the tool.
Build and run the container:

```
docker build . -t rat
docker run -w /home/rat -it rat /bin/bash
```

### From Sources

1. Install dependencies above.

   The Antlr tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar and can be installed by:

```
cd /usr/local/lib
curl -O https://www.antlr.org/download/antlr-4.10.1-complete.jar
```

2. Build the tool

```
cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build
cmake --build ./build --target rat -j 10
```

## Usage

RAT supports proof files written in relational algebra as in CAT.
Proof files support the following expressions to describe sets and relations.
Relation identifiers must begin with a lowercase letter and set identifiers must begin with an upper case letter.

```
<set>       ::= <base_set> | <set_identifier> | '0' | <set> '&' <set> | <set> '|' <set> 
    
<relation>  ::= <base_relation> | <relation_identifier> | '0' | 'id'
            | <relation> '&' <relation> | <relation> '|' <relation> | <relation> '^-1'
            | <relation> ';' <relation> | <relation> '^*' | <relation> '^+' | <relation> '?'	
```

A proof file consist of statements fo the following form.

```
<inlude>        ::= 'include' <filepath>
<definition>    ::= 'let' <identifier> '=' <expression>
<assumption>    ::= 'assume' <expression> '<=' <expression>
<assertion>     ::= 'assert' <expression> '<=' <expression>
```

There are only five supported types of assumptions:

```
'assert' <relation> '<=' '0'
'assert' <relation> '<=' <base_relation>
'assert' <relation> '<=' 'id'
'assert' <set> '<=' '0'
'assert' <set> '<=' <base_set>
```

An example proof file could be:

```
assume a;a <= 0
assert (a;b;a) & id <= 0
```

To run the tool execute:

```
./build/rat <path to proof file>
```

There are two possible results for each assertion in a proof file:

- True: the assertion holds
- False: the assertion does not hold

The tool generates either a proof or a counterexample.
Both are exported as .dot file in the output folder.
