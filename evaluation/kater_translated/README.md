This directory contains a subset of Kater's tests (taken from their github repo) translated to RAT.
It mainly focuses on a few equivalence tests that were evaluated in the Kater paper.

NOTE:
    (1) Most compilation tests require unsupported types of assumptions (e.g. r1 <= r2 where r2 is complex) and thus are excluded.
    (2) Only correct equivalence tests are translated.
        - This is a best effort translation because Kater uses a lot of hidden assumptions that need to be made explicit,
          some of which are unexpected like SC <= ACQ (REL)