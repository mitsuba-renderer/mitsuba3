#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <drjit/texture_impl.h>
#include <cctype>
#include <cstdlib>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-math:

Math expression texture (:monosp:`math`)
----------------------------------------

.. pluginparameters::

 * - expr
   - |string|
   - Expression to evaluate using the simple language described below.

 * - (Nested plugin)
   - |texture|
   - Input textures. The expression refers to them as ``in[0]``, ``in[1]``,
     etc., numbered in the order of declaration. In the scene parameter API,
     they are exposed as differentiable parameters named ``in0``, ``in1``, ...
   - |exposed|, |differentiable|

This texture evaluates a mathematical expression involving an arbitrary number
of input textures. This can be helpful to transform textures in ways that are
not covered by any existing plugin.

The expression consists of an optional sequence of assignments to temporaries
``tmp[0]``, ``tmp[1]``, ..., separated by semicolons, followed by a final
expression that produces the result of the texture. Each temporary must be
assigned exactly once before use. For example:

.. code-block:: text

    tmp[0] = clip(in[0], 0, 1); lerp(in[1], in[2], tmp[0])

The language provides

- The arithmetic operators ``+``, ``-``, ``*``, ``/``, unary ``-``, and
  parentheses.

- Comparisons ``<``, ``<=``, ``>``, ``>=``, ``==``, ``!=`` and the ternary
  operator ``cond ? a : b`` to consume them.

- The logical operators ``&&``, ``||``, and ``!``, which treat any nonzero
  operand as true.

- Unary functions: ``abs``, ``sign``, ``sqrt``, ``cbrt``, ``rcp``, ``rsqrt``,
  ``erf``, ``sin``, ``cos``, ``tan``, ``asin``, ``acos``, ``atan``, ``sinh``,
  ``cosh``, ``tanh``, ``asinh``, ``acosh``, ``atanh``, ``exp``, ``log``,
  ``exp2``, ``log2``, ``round``, ``trunc``, ``floor``, ``ceil``.

- Binary and ternary functions: ``min``, ``max``, ``pow``, ``atan2``,
  ``fmod``, ``lerp(a, b, t)``, ``clip(x, lo, hi)``, ``fma(a, b, c)``.

- Floating point literals and the constants ``pi`` and ``e``.

Operator precedence follows the C language. The language only supports floating
point values and represents Boolean values as ``0.0`` and ``1.0``.
Monochromatic, trichromatic, and spectral texture queries apply the expression
separately to each input channel.

The plugin compiles the expression into a compact stack machine bytecode
representation that is interpreted during texture evaluation. In JIT-compiled
variants, this interpretation runs once at kernel trace time, hence the
evaluation cost matches that of directly written arithmetic.

.. tabs::
    .. code-tab:: xml
        :name: math-texture

        <texture type="math">
            <string name="expr" value="in[0] * (1 - in[1])"/>
            <texture type="bitmap">
                <string name="filename" value="texture.png"/>
            </texture>
            <texture type="checkerboard"/>
        </texture>

    .. code-tab:: python

        'type': 'math',
        'expr': 'in[0] * (1 - in[1])',
        'input_0': { 'type': 'bitmap', 'filename': 'texture.png' },
        'input_1': { 'type': 'checkerboard' }

 */

namespace {

// Functions available in the expression language
#define MI_MATH_UNARY_OPS(F)                                                   \
    F(abs) F(sign) F(sqrt) F(cbrt) F(rcp) F(rsqrt) F(erf)                      \
    F(sin) F(cos) F(tan) F(asin) F(acos) F(atan)                               \
    F(sinh) F(cosh) F(tanh) F(asinh) F(acosh) F(atanh)                         \
    F(exp) F(log) F(exp2) F(log2) F(round) F(trunc) F(floor) F(ceil)

#define MI_MATH_BINARY_OPS(F)                                                  \
    F(min, minimum) F(max, maximum) F(pow, pow) F(atan2, atan2) F(fmod, fmod)

#define MI_MATH_TERNARY_OPS(F)                                                 \
    F(lerp, lerp) F(clip, clip) F(fma, fmadd)

enum class Op : uint8_t {
    // Push a constant or memory slot, pop the stack top into a memory slot
    Const, Load, Store,

    // Operators
    Add, Sub, Mul, Div, Neg,
    Lt, Le, Gt, Ge, Eq, Ne,
    Select

    #define F1(name) , name
    #define F2(name, drname) , name
    MI_MATH_UNARY_OPS(F1)
    MI_MATH_BINARY_OPS(F2)
    MI_MATH_TERNARY_OPS(F2)
    #undef F1
    #undef F2
};

struct Inst { Op op; uint8_t imm; };
static_assert(sizeof(Inst) == 2);

struct Fn { const char *name; Op op; uint8_t arity; };

static const Fn fn_table[] = {
    #define F1(name)         { #name, Op::name, 1 },
    #define F2(name, drname) { #name, Op::name, 2 },
    #define F3(name, drname) { #name, Op::name, 3 },
    MI_MATH_UNARY_OPS(F1)
    MI_MATH_BINARY_OPS(F2)
    MI_MATH_TERNARY_OPS(F3)
    #undef F1
    #undef F2
    #undef F3
};

/// Binary operator precedence levels, following the C language
enum Prec : int { PrecOr = 1, PrecAnd, PrecCmp, PrecSum, PrecTerm };

struct BinOp { const char *tok; int prec; Op op; };

// Binary operators. Two-character operators precede their one-character
// prefixes so that the longest match wins. '&&' and '||' compile to a
// product/maximum of 0/1 truth values (see MathParser::binary()).
static const BinOp bin_ops[] = {
    { "||", PrecOr,   Op::max }, { "&&", PrecAnd,  Op::Mul },
    { "==", PrecCmp,  Op::Eq  }, { "!=", PrecCmp,  Op::Ne  },
    { "<=", PrecCmp,  Op::Le  }, { ">=", PrecCmp,  Op::Ge  },
    { "<",  PrecCmp,  Op::Lt  }, { ">",  PrecCmp,  Op::Gt  },
    { "+",  PrecSum,  Op::Add }, { "-",  PrecSum,  Op::Sub },
    { "*",  PrecTerm, Op::Mul }, { "/",  PrecTerm, Op::Div }
};

/// Recursive descent parser to compile an expression into stack machine
/// bytecode. The language grammar is
///
///     program := ( 'tmp[' num ']' '=' expr ';' )* expr
///     expr    := binary ( '?' expr ':' expr )?
///     binary  := unary ( <binary operator> unary )*
///     unary   := ( '-' | '!' )* primary
///     primary := '(' expr ')' | number | 'pi' | 'e' | 'in[' num ']'
///                | 'tmp[' num ']' | function '(' expr ( ',' expr )* ')'
///
/// Parsing and code generation happen in a single pass: each rule above emits
/// code that leaves exactly one value on the operand stack. The inputs and the
/// ``tmp[..]`` temporaries occupy fixed memory slots that the ``Load`` and
/// ``Store`` instructions reference by index.
struct MathParser {
    const char *start, *p;      // Start of the input text, current read position
    size_t n_inputs;            // Number of textures that ``in[..]`` may reference
    std::vector<Inst> code;     // Generated bytecode
    std::vector<double> consts; // Constant pool referenced by ``Op::Const``
    bool assigned[256] { };     // Temporaries that were assigned so far
    uint32_t n_tmp = 0;         // Number of temporary slots in use
    uint32_t stack_size = 0;    // Worst-case operand stack size
    int depth = 0;              // Operand stack depth at the current instruction

    MathParser(const char *expr, size_t n_inputs)
        : start(expr), p(expr), n_inputs(n_inputs) { }

    /// Sequence of ``tmp[..]`` assignments followed by a result expression
    void program() {
        int k;
        while ((k = assignment()) != -1) {
            expr();
            emit(Op::Store, -1, tmp_slot((uint8_t) k));
            assigned[k] = true;
            n_tmp = std::max(n_tmp, (uint32_t) k + 1);
            if (!match(';'))
                fail("expected ';'");
        }

        whitespace();
        if (*p == '\0')
            fail("the program must end with a result expression");

        expr();
        whitespace();
        if (*p == ';')
            fail("only the final statement may be a bare expression");
        if (*p != '\0')
            fail("expected end of input");

        Assert(depth == 1);
    }

    /// Try to consume an assignment prefix ``tmp[k] =`` and return ``k``. If
    /// the upcoming text is not an assignment, restore the position and
    /// return -1 so that it can be parsed as the result expression instead.
    int assignment() {
        whitespace();
        const char *save = p;
        if (ident() == "tmp") {
            whitespace();
            if (*p == '[') {
                uint8_t k = index();
                whitespace();
                // Exclude '==', which starts a comparison
                if (p[0] == '=' && p[1] != '=') {
                    ++p;
                    if (assigned[k]) {
                        p = save;
                        fail(tfm::format("tmp[%i] is assigned twice", k));
                    }
                    return k;
                }
            }
        }
        p = save;
        return -1;
    }

    /// Parse a complete expression. This level handles the ternary
    /// conditional, which binds more loosely than the binary operators.
    void expr() {
        binary(PrecOr);
        if (match('?')) {
            expr();
            if (!match(':'))
                fail("expected ':'");
            expr();
            emit(Op::Select, -2);
        }
    }

    /// Left-associative binary operators, parsed via precedence climbing
    void binary(int min_prec) {
        unary();
        bool seen_cmp = false;
        while (const BinOp *b = match_op(min_prec)) {
            // The comparison operators are non-associative
            if (b->prec == PrecCmp) {
                if (seen_cmp)
                    fail("comparison operators cannot be chained");
                seen_cmp = true;
            }

            // '&&' and '||' reduce to a product/maximum of 0/1 truth values
            bool logical = b->prec <= PrecAnd;
            if (logical)
                cmp_zero(Op::Ne);

            // The right operand absorbs subsequent operators that bind more
            // tightly, which also makes equal-precedence chains left-associative
            binary(b->prec + 1);

            if (logical)
                cmp_zero(Op::Ne);
            emit(b->op, -1);
        }
    }

    /// Unary minus and logical negation
    void unary() {
        whitespace();
        if (*p == '-') {
            ++p;
            unary();
            emit(Op::Neg, 0);
        } else if (*p == '!') {
            // Compile '!x' into 'x == 0'
            ++p;
            unary();
            cmp_zero(Op::Eq);
        } else {
            primary();
        }
    }

    /// Literals, constants, ``in``/``tmp`` references, calls, and parentheses
    void primary() {
        whitespace();
        char c = *p;
        if (c == '(') {
            ++p;
            expr();
            if (!match(')'))
                fail("expected ')'");
        } else if (std::isdigit((unsigned char) c) || c == '.') {
            char *end;
            double value = string::parse_float<double>(p, p + std::strlen(p), &end);
            if (end == p)
                fail("invalid number");
            p = end;
            emit(Op::Const, 1, const_index(value));
        } else if (std::isalpha((unsigned char) c) || c == '_') {
            const char *save = p;
            std::string_view name = ident();
            if (name == "in") {
                uint8_t k = index();
                if (k >= n_inputs)
                    fail(tfm::format("in[%i] exceeds the number of declared "
                                     "inputs (%zu)", k, n_inputs));
                emit(Op::Load, 1, k);
            } else if (name == "tmp") {
                uint8_t k = index();
                if (!assigned[k])
                    fail(tfm::format("tmp[%i] is read before being assigned", k));
                emit(Op::Load, 1, tmp_slot(k));
            } else if (name == "pi") {
                emit(Op::Const, 1, const_index(dr::Pi<double>));
            } else if (name == "e") {
                emit(Op::Const, 1, const_index(dr::E<double>));
            } else {
                const Fn *fn = nullptr;
                for (const Fn &f : fn_table) {
                    if (name == f.name) {
                        fn = &f;
                        break;
                    }
                }
                if (!fn) {
                    p = save;
                    fail(tfm::format("unknown identifier \"%s\"", std::string(name)));
                }
                if (!match('('))
                    fail("expected '('");
                int argc = 0;
                do {
                    expr();
                    ++argc;
                } while (match(','));
                if (!match(')'))
                    fail("expected ')' or ','");
                if (argc != fn->arity) {
                    p = save;
                    fail(tfm::format("function \"%s\" takes %i argument(s), got %i",
                                     fn->name, fn->arity, argc));
                }
                emit(fn->op, 1 - fn->arity);
            }
        } else if (c == '\0') {
            fail("unexpected end of input");
        } else {
            fail("expected an operand");
        }
    }

    /// Skip whitespace
    void whitespace() {
        while (std::isspace((unsigned char) *p))
            ++p;
    }

    /// Consume the character ``c`` if it follows optional whitespace
    bool match(char c) {
        whitespace();
        if (*p != c)
            return false;
        ++p;
        return true;
    }

    /// Consume a binary operator with precedence of at least ``min_prec``. A
    /// weaker operator is left in place for an enclosing ``binary()`` call.
    const BinOp *match_op(int min_prec) {
        whitespace();
        for (const BinOp &b : bin_ops) {
            size_t n = std::strlen(b.tok);
            if (std::strncmp(p, b.tok, n) == 0) {
                if (b.prec < min_prec)
                    return nullptr;
                p += n;
                return &b;
            }
        }
        return nullptr;
    }

    /// Consume an identifier, which may turn out to be empty
    std::string_view ident() {
        const char *q = p;
        while (std::isalnum((unsigned char) *p) || *p == '_')
            ++p;
        return { q, (size_t) (p - q) };
    }

    /// Parse a bracketed index following ``in`` or ``tmp``
    uint8_t index() {
        if (!match('['))
            fail("expected '['");
        whitespace();
        if (!std::isdigit((unsigned char) *p))
            fail("expected an index");
        char *end;
        long value = std::strtol(p, &end, 10);
        p = end;
        if (value > 255)
            fail("index too large");
        if (!match(']'))
            fail("expected ']'");
        return (uint8_t) value;
    }

    /// Append an instruction and track the resulting operand stack depth
    void emit(Op op, int delta, uint8_t imm = 0) {
        code.push_back({ op, imm });
        depth += delta;
        stack_size = std::max(stack_size, (uint32_t) depth);
    }

    /// Constant pool index of ``value``, appending it on first use
    uint8_t const_index(double value) {
        for (size_t i = 0; i < consts.size(); ++i)
            if (consts[i] == value)
                return (uint8_t) i;
        if (consts.size() == 256)
            fail("too many constants");
        consts.push_back(value);
        return (uint8_t) (consts.size() - 1);
    }

    /// Memory slot of temporary ``k``, which is stored after the inputs
    uint8_t tmp_slot(uint8_t k) {
        size_t slot = n_inputs + k;
        if (slot > 255)
            fail("too many inputs and temporaries");
        return (uint8_t) slot;
    }

    /// Compare the value at the top of the stack against zero, giving 0 or 1
    void cmp_zero(Op op) {
        emit(Op::Const, 1, const_index(0.0));
        emit(op, -1);
    }

    /// Report a parse error at the current position
    [[noreturn]] void fail(const std::string &msg) const {
        // Display the offending line with a caret marking the error position
        const char *line = p, *end = p;
        while (line > start && line[-1] != '\n')
            --line;
        while (*end && *end != '\n')
            ++end;
        Throw("Error while parsing math texture expression (offset %zu): %s\n\n"
              "    %s\n    %s^", (size_t) (p - start), msg,
              std::string(line, end), std::string((size_t) (p - line), ' '));
    }
};

} // namespace

template <typename Float, typename Spectrum>
class MathTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    MathTexture(const Properties &props) : Texture(props) {
        for (const auto &prop : props.objects()) {
            if (Texture *texture = prop.try_get<Texture>())
                m_inputs.push_back(texture);
        }

        m_expression = props.get<std::string>("expr");
        MathParser parser(m_expression.c_str(), m_inputs.size());
        parser.program();

        m_code = std::move(parser.code);
        m_consts.assign(parser.consts.begin(), parser.consts.end());
        m_tmp_count = parser.n_tmp;
        m_stack_size = parser.stack_size;
    }

    void traverse(TraversalCallback *cb) override {
        for (size_t i = 0; i < m_inputs.size(); ++i)
            cb->put("in" + std::to_string(i), m_inputs[i],
                    ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return run([&](uint32_t i) { return m_inputs[i]->eval(si, active); });
    }

    Float eval_1(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return run([&](uint32_t i) { return m_inputs[i]->eval_1(si, active); });
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return run([&](uint32_t i) { return m_inputs[i]->eval_3(si, active); });
    }

    bool is_spatially_varying() const override {
        for (const auto &input : m_inputs)
            if (input->is_spatially_varying())
                return true;
        return false;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MathTexture[" << std::endl
            << "  expression = \"" << m_expression << "\"," << std::endl;
        for (size_t i = 0; i < m_inputs.size(); ++i)
            oss << "  in" << i << " = " << string::indent(m_inputs[i]) << "," << std::endl;
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(MathTexture)

protected:
    /// Interpret the bytecode. The value type is set by the input evaluation
    /// callback and may be a monochromatic, RGB, or spectral quantity.
    template <typename F> auto run(F &&eval_input) const {
        using T = decltype(eval_input(0u));

        uint32_t n_inputs = (uint32_t) m_inputs.size(),
                 n = n_inputs + m_tmp_count + m_stack_size;

        T *mem = (T *) alloca(n * sizeof(T));
        dr::detail::tex_scratch<T> scratch(mem, n);

        for (uint32_t i = 0; i < n_inputs; ++i)
            mem[i] = eval_input(i);

        T *sp = mem + n_inputs + m_tmp_count;

        for (Inst inst : m_code) {
            uint8_t imm = inst.imm;
            switch (inst.op) {
                case Op::Const: *sp++ = T(m_consts[imm]); break;
                case Op::Load:  *sp++ = mem[imm]; break;
                case Op::Store: mem[imm] = *--sp; break;

                case Op::Add: --sp; sp[-1] += sp[0]; break;
                case Op::Sub: --sp; sp[-1] -= sp[0]; break;
                case Op::Mul: --sp; sp[-1] *= sp[0]; break;
                case Op::Div: --sp; sp[-1] /= sp[0]; break;
                case Op::Neg: sp[-1] = -sp[-1]; break;

                case Op::Lt: --sp; sp[-1] = dr::select(sp[-1] <  sp[0], T(1), T(0)); break;
                case Op::Le: --sp; sp[-1] = dr::select(sp[-1] <= sp[0], T(1), T(0)); break;
                case Op::Gt: --sp; sp[-1] = dr::select(sp[-1] >  sp[0], T(1), T(0)); break;
                case Op::Ge: --sp; sp[-1] = dr::select(sp[-1] >= sp[0], T(1), T(0)); break;
                case Op::Eq: --sp; sp[-1] = dr::select(sp[-1] == sp[0], T(1), T(0)); break;
                case Op::Ne: --sp; sp[-1] = dr::select(sp[-1] != sp[0], T(1), T(0)); break;

                case Op::Select: sp -= 2; sp[-1] = dr::select(sp[-1] != T(0), sp[0], sp[1]); break;

                #define F1(name)         case Op::name: sp[-1] = dr::name(sp[-1]); break;
                #define F2(name, drname) case Op::name: --sp; sp[-1] = dr::drname(sp[-1], sp[0]); break;
                #define F3(name, drname) case Op::name: sp -= 2; sp[-1] = dr::drname(sp[-1], sp[0], sp[1]); break;
                MI_MATH_UNARY_OPS(F1)
                MI_MATH_BINARY_OPS(F2)
                MI_MATH_TERNARY_OPS(F3)
                #undef F1
                #undef F2
                #undef F3
            }
        }

        return sp[-1];
    }

    std::string m_expression;
    std::vector<ref<Texture>> m_inputs;
    std::vector<Inst> m_code;
    std::vector<ScalarFloat> m_consts;
    uint32_t m_tmp_count = 0, m_stack_size = 0;

    MI_TRAVERSE_CB(Texture, m_inputs)
};

MI_EXPORT_PLUGIN(MathTexture)
NAMESPACE_END(mitsuba)
