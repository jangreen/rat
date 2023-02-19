#include <unordered_set>

class ProofRule
{
public:
    enum Rule
    {
        none,
        axiomEmpty,
        axiomFull,
        axiomEqual,
        andLeft,
        andRight,
        orLeft,
        orRight,
        inverse,
        distributiveCup,
        distributiveCap, // TODO
        seqLeft,
        simplifyTc,
        transitiveClosure,
        unroll,
        cons,
        weakRight,
        weakLeft,
        loop,
        empty
    };

    ProofRule() = delete;
    constexpr ProofRule(Rule rule) : rule(rule) {}
    constexpr operator Rule() const { return rule; }
    explicit operator bool() const = delete;
    constexpr bool operator==(ProofRule other) const { return rule == other.rule; }
    constexpr bool operator!=(ProofRule other) const { return rule != other.rule; }

    constexpr string_view toString()
    {
        switch (rule)
        {
        case Rule::none:
            return string_view("none");
        case Rule::axiomEmpty:
            return string_view("axiomEmpty");
        case Rule::axiomFull:
            return string_view("axiomFull");
        case Rule::axiomEqual:
            return string_view("axiomEqual");
        case Rule::andLeft:
            return string_view("andLeft");
        case Rule::andRight:
            return string_view("andRight");
        case Rule::orLeft:
            return string_view("orLeft");
        case Rule::orRight:
            return string_view("orRight");
        case Rule::inverse:
            return string_view("inverse");
        case Rule::distributiveCup:
            return string_view("distributiveCup");
        case Rule::distributiveCap:
            return string_view("distributiveCap");
        case Rule::seqLeft:
            return string_view("seqLeft");
        case Rule::simplifyTc:
            return string_view("simplifyTc");
        case Rule::transitiveClosure:
            return string_view("transitiveClosure");
        case Rule::unroll:
            return string_view("unroll");
        case Rule::cons:
            return string_view("cons");
        case Rule::weakRight:
            return string_view("weakRight");
        case Rule::weakLeft:
            return string_view("weakLeft");
        case Rule::loop:
            return string_view("loop");
        case Rule::empty:
            return string_view("empty");
        }
    }

    // iff rules // TODO: incomplete yet
    constexpr bool iffRule()
    {
        switch (rule)
        {
        case Rule::none:
            return false;
        case Rule::axiomEmpty:
            return true;
        case Rule::axiomFull:
            return true;
        case Rule::axiomEqual:
            return true;
        case Rule::andLeft:
            return true;
        case Rule::andRight:
            return true;
        case Rule::orLeft:
            return true;
        case Rule::orRight:
            return true;
        case Rule::inverse:
            return false;
        case Rule::distributiveCup:
            return true;
        case Rule::distributiveCap:
            return false;
        case Rule::seqLeft:
            return false;
        case Rule::simplifyTc:
            return false;
        case Rule::transitiveClosure:
            return false;
        case Rule::unroll:
            return false;
        case Rule::cons:
            return false;
        case Rule::weakRight:
            return false;
        case Rule::weakLeft:
            return false;
        case Rule::loop:
            return false;
        case Rule::empty:
            return false;
        }
    }

private:
    Rule rule;
};
