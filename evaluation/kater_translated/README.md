This directory contains a subset of Kater's tests (taken from their github repo) translated to RAT.

NOTE:
    (1) Some compilation tests require unsupported types of assumptions (e.g. r1 <= r2 where r2 is complex) and thus are excluded.
    (2) Most compilation tests related to power have the shape "assert m & id <= power" where "power" contains id,
        making them trivial. We have tried to fix this error by changing "eco*;po?;eco*" (which contains id) 
        to (the probably intended) "eco;po?;eco*".
    (3) Some test files were split into two.