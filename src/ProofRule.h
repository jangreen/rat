#include <string_view>
class ProofRule
{
public:
    enum Rule
    {
        none,
        bottom,
        empty,
        id,
        negId,
        propagation,
        at,
        negAt,
        intersection,
        negIntersection,
        composition,
        negComposition,
        transitiveClosure,
        negTransitiveClosure,
        choice,
        negChoice,
        a,
        negA,
        converseA,
        negConverseA
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
            return string_view("(none)");
        case Rule::bottom:
            return string_view("(bot)");
        case Rule::empty:
            return string_view("(empty)");
        case Rule::id:
            return string_view("(id)");
        case Rule::negId:
            return string_view("(-id)");
        case Rule::propagation:
            return string_view("(=)");
        case Rule::at:
            return string_view("(@)");
        case Rule::negAt:
            return string_view("(-@)");
        case Rule::intersection:
            return string_view("(cap)");
        case Rule::negIntersection:
            return string_view("(-cap)");
        case Rule::composition:
            return string_view("(;)");
        case Rule::negComposition:
            return string_view("(-;)");
        case Rule::transitiveClosure:
            return string_view("(*)");
        case Rule::negTransitiveClosure:
            return string_view("(-*)");
        case Rule::choice:
            return string_view("(cup)");
        case Rule::negChoice:
            return string_view("(-cup)");
        case Rule::a:
            return string_view("(a)");
        case Rule::negA:
            return string_view("(-a)");
        case Rule::converseA:
            return string_view("(a^-1)");
        case Rule::negConverseA:
            return string_view("(-a^-1)");
        }
    }

private:
    Rule rule;
};
