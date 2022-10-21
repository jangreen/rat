

class ProofRule
{
public:
    enum Rule
    {
        none,
        axiomEmpty,
        axiomFull,
        axiomEqual,
        axiomTheory,
        andLeft,
        andRight,
        orLeft,
        orRight,
        inverse,
        seqLeft,
        transitiveClosure,
        unroll,
        cut,
        cons,
        weakRight,
        weakLeft,
        loop,
        empty
    };

    ProofRule() = default;
    constexpr ProofRule(Rule rule) : rule(rule) {}
    constexpr operator Rule() const { return rule; }
    explicit operator bool() const = delete;
    constexpr bool operator==(ProofRule other) const { return rule == other.rule; }
    constexpr bool operator!=(ProofRule other) const { return rule != other.rule; }

    string toString()
    {
        switch (rule)
        {
        case Rule::none:
            return "none";
        case Rule::axiomEmpty:
            return "axiomEmpty";
        case Rule::axiomFull:
            return "axiomFull";
        case Rule::axiomEqual:
            return "axiomEqual";
        case Rule::axiomTheory:
            return "axiomTheory";
        case Rule::andLeft:
            return "andLeft";
        case Rule::andRight:
            return "andRight";
        case Rule::orLeft:
            return "orLeft";
        case Rule::orRight:
            return "orRight";
        case Rule::inverse:
            return "inverse";
        case Rule::seqLeft:
            return "seqLeft";
        case Rule::transitiveClosure:
            return "transitiveClosure";
        case Rule::unroll:
            return "unroll";
        case Rule::cut:
            return "cut";
        case Rule::cons:
            return "cons";
        case Rule::weakRight:
            return "weakRight";
        case Rule::weakLeft:
            return "weakLeft";
        case Rule::loop:
            return "loop";
        default:
            return "error";
        }
    }

private:
    Rule rule;
};