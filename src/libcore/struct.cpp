#include <mitsuba/core/struct.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/hash.h>
#include <mitsuba/core/jit.h>
#include <enoki/array.h>
#include <enoki/half.h>
#include <enoki/color.h>
#include <unordered_map>
#include <ostream>
#include <map>

/// Set this to '1' to view generated conversion code
#if !defined(MTS_JIT_LOG_ASSEMBLY)
#  define MTS_JIT_LOG_ASSEMBLY 0
#endif

NAMESPACE_BEGIN(mitsuba)

// Defined in dither-matrix256.cpp
extern const float dither_matrix256[65536];

NAMESPACE_BEGIN(detail)

#if defined(DOUBLE_PRECISION)
#  define Float double
#else
#  define Float float
#endif

#if MTS_STRUCTCONVERTER_USE_JIT == 1

using namespace asmjit;

/// Helper class used to JIT-compile conversion code (in the StructCompiler class)
class StructCompiler {
public:
    // A variable can exist in various forms (integer/float/gamma corrected..)
    struct Key {
        std::string name;
        Struct::Type type;
        uint32_t flags;

        bool operator<(const Key &o) const {
            return std::make_tuple(name, type, flags) < std::make_tuple(o.name, o.type, o.flags);
        }
    };

    // .. and it is either stored in a general purpose or a vector register
    struct Value {
        X86Gp gp;
        X86Xmm xmm;
    };

    StructCompiler(X86Compiler &cc, X86Gp x, X86Gp y, bool dither, Label &err_label)
        : cc(cc), xp(x), yp(y), dither(dither), err_label(err_label) { }


    /* --------------------------------------------------------- */
    /*  Various low-level aliases for instructions that adapt     */
    /*  to the working precision and available instruction sets  */
    /* --------------------------------------------------------- */

    template <typename T>
    X86Mem const_(T value) {
        #if !defined(DOUBLE_PRECISION)
            return cc.newFloatConst(asmjit::kConstScopeGlobal, (float) value);
        #else
            return cc.newDoubleConst(asmjit::kConstScopeGlobal, (double) value);
        #endif
    }

    /// Floating point comparison
    template <typename X, typename Y>
    void ucomis(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vucomiss(x, y);
            #else
                cc.vucomisd(x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.ucomiss(x, y);
            #else
                cc.ucomisd(x, y);
            #endif
        #endif
    }

    /// Floating point move operation
    template <typename X, typename Y> void movs(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vmovss(x, y);
            #else
                cc.vmovsd(x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.movss(x, y);
            #else
                cc.movsd(x, y);
            #endif
        #endif
    }

    void movs(const X86Xmm &x, const X86Xmm &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vmovss(x, x, y);
            #else
                cc.vmovsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.movss(x, y);
            #else
                cc.movsd(x, y);
            #endif
        #endif
    }

    /// Floating point blend operation
    template <typename X, typename Y, typename Z>
    void blend(const X &x, const Y &y, const Z &mask) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vblendvps(x, x, y, mask);
            #else
                cc.vblendvpd(x, x, y, mask);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.blendvps(x, y, mask);
            #else
                cc.blendvpd(x, y, mask);
            #endif
        #endif
    }

    /// Floating point comparison
    template <typename X, typename Y>
    void cmps(const X &x, const Y &y, int mode) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vcmpss(x, x, y, Imm(mode));
            #else
                cc.vcmpsd(x, x, y, Imm(mode));
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.cmpss(x, y, Imm(mode));
            #else
                cc.cmpsd(x, y, Imm(mode));
            #endif
        #endif
    }

    /// Floating point addition operation (2op)
    template <typename X, typename Y>
    void adds(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vaddss(x, x, y);
            #else
                cc.vaddsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.addss(x, y);
            #else
                cc.addsd(x, y);
            #endif
        #endif
    }

    /// Floating point subtraction operation (3op)
    template <typename X, typename Y, typename Z>
    void subs(const X &x, const Y &y, const Z &z) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vsubss(x, y, z);
            #else
                cc.vsubsd(x, y, z);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.movss(x, y);
                cc.subss(x, z);
            #else
                cc.movsd(x, y);
                cc.subsd(x, z);
            #endif
        #endif
    }

    /// Floating point multiplication operation (2op)
    template <typename X, typename Y>
    void muls(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vmulss(x, x, y);
            #else
                cc.vmulsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.mulss(x, y);
            #else
                cc.mulsd(x, y);
            #endif
        #endif
    }

    /// Floating point multiplication operation (3op)
    template <typename X, typename Y, typename Z>
    void muls(const X &x, const Y &y, const Z &z) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vmulss(x, y, z);
            #else
                cc.vmulsd(x, y, z);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.movss(x, y);
                cc.mulss(x, z);
            #else
                cc.movsd(x, y);
                cc.mulsd(x, z);
            #endif
        #endif
    }

    /// Floating point division operation (2op)
    template <typename X, typename Y>
    void divs(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vdivss(x, x, y);
            #else
                cc.vdivsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.divss(x, y);
            #else
                cc.divsd(x, y);
            #endif
        #endif
    }

    /// Floating point square root operation
    template <typename X, typename Y>
    void sqrts(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vsqrtss(x, x, y);
            #else
                cc.vsqrtsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.sqrtss(x, y);
            #else
                cc.sqrtsd(x, y);
            #endif
        #endif
    }

    /// Floating point maximum operation
    template <typename X, typename Y>
    void maxs(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vmaxss(x, x, y);
            #else
                cc.vmaxsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.maxss(x, y);
            #else
                cc.maxsd(x, y);
            #endif
        #endif
    }

    /// Floating point minimum operation
    template <typename X, typename Y>
    void mins(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vminss(x, x, y);
            #else
                cc.vminsd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.minss(x, y);
            #else
                cc.minsd(x, y);
            #endif
        #endif
    }

    /// Floating point fused multiply-add operation
    template <typename X, typename Y, typename Z>
    void fmadd213(const X &x, const Y &y, const Z &z) {
        #if defined(ENOKI_X86_AVX) && defined(ENOKI_X86_FMA)
            #if !defined(DOUBLE_PRECISION)
                cc.vfmadd213ss(x, y, z);
            #else
                cc.vfmadd213sd(x, y, z);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.mulss(x, y);
                cc.addss(x, z);
            #else
                cc.mulsd(x, y);
                cc.addsd(x, z);
            #endif
        #endif
    }

    /// Floating point fused multiply-add operation
    template <typename X, typename Y, typename Z>
    void fmadd231(const X &x, const Y &y, const Z &z) {
        #if defined(ENOKI_X86_AVX) && defined(ENOKI_X86_FMA)
            #if !defined(DOUBLE_PRECISION)
                cc.vfmadd231ss(x, y, z);
            #else
                cc.vfmadd231sd(x, y, z);
            #endif
        #else
            auto tmp = cc.newXmm();
            #if !defined(DOUBLE_PRECISION)
                cc.movss(tmp, y);
                cc.mulss(tmp, z);
                cc.addss(x, tmp);
            #else
                cc.movsd(tmp, y);
                cc.mulsd(tmp, z);
                cc.addsd(x, tmp);
            #endif
        #endif
    }

    /// Convert an integer to a floating point value
    template <typename X, typename Y>
    void cvtsi2s(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vcvtsi2ss(x, x, y);
            #else
                cc.vcvtsi2sd(x, x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.cvtsi2ss(x, y);
            #else
                cc.cvtsi2sd(x, y);
            #endif
        #endif
    }

    /// Convert a floating point value to an integer (truncating)
    template <typename X, typename Y>
    void cvts2si(const X &x, const Y &y) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vcvtss2si(x, y);
            #else
                cc.vcvtsd2si(x, y);
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.cvtss2si(x, y);
            #else
                cc.cvtsd2si(x, y);
            #endif
        #endif
    }

    /// Floating point rounding operation
    template <typename X, typename Y>
    void rounds(const X &x, const Y &y, int mode) {
        #if defined(ENOKI_X86_AVX)
            #if !defined(DOUBLE_PRECISION)
                cc.vroundss(x, x, y, Imm(mode));
            #else
                cc.vroundsd(x, x, y, Imm(mode));
            #endif
        #else
            #if !defined(DOUBLE_PRECISION)
                cc.roundss(x, y, Imm(mode));
            #else
                cc.roundsd(x, y, Imm(mode));
            #endif
        #endif
    }

    /// Forward/inverse gamma correction using the sRGB profile
    X86Xmm gamma(X86Xmm x, bool to_srgb) {
        #if MTS_JIT_LOG_ASSEMBLY == 1
            cc.comment(to_srgb ? "# Linear -> sRGB conversion"
                               : "# sRGB -> linear conversion");
        #endif

        X86Xmm a = cc.newXmm(),
               b = cc.newXmm();

        movs(a, const_(to_srgb ? 12.92 : (1.0 / 12.92)));
        ucomis(x, const_(to_srgb ? 0.0031308 : 0.04045));

        Label low_value = cc.newLabel();
        cc.jb(low_value);

        X86Xmm y;
        if (to_srgb) {
           y = cc.newXmm();
           sqrts(y, x);
        } else {
            y = x;
        }

        #if !defined(DOUBLE_PRECISION)
            // Rational polynomial fit, rel.err = 2*10^-7
            float to_srgb_coeffs[2][6] =
              { { -0.016202083165206348f, 0.7551545191665577f, 2.0041169284241644f,
                  0.7642611304733891f, 0.03453868659826638f,
                  -0.0016829072605308378f },
                { 1.f, 1.8970238036421054f, 0.6085338522168684f,
                  0.03467195408529984f, -0.00004375359692957097f,
                  4.178892964897981e-7f } };
        #else
            // Rational polynomial fit, rel.err = 8*10^-15
            double to_srgb_coeffs[2][11] =
              { { -0.0031151377052754843, 0.5838023820686707, 8.450947414259522,
                  27.901125077137042, 32.44669922192121, 15.374469584296442,
                  3.0477578489880823, 0.2263810267005674, 0.002531335520959116,
                  -0.00021805827098915798, -3.7113872202050023e-6 },
                { 1., 10.723011300050162, 29.70548706952188, 30.50364355650628,
                  13.297981743005433, 2.575446652731678, 0.21749170309546628,
                  0.007244514696840552, 0.00007045228641004039,
                  -8.387527630781522e-9, 2.2380622409188757e-11 } };
        #endif

        #if !defined(DOUBLE_PRECISION)
            // Rational polynomial fit, rel.err = 2*10^-7
            float from_srgb_coeffs[2][5] =
                { { -36.04572663838034f, -47.46726633009393f, -11.199318357635072f,
                    -0.7386328024653209f, -0.0163933279112946f },
                  { 1.f, -18.225745396846637f, -59.096406619244426f,
                    -19.140923959601675f, -0.004261480793199332f } };
        #else
            // Rational polynomial fit, rel.err = 1.5*10^-15
            double from_srgb_coeffs[2][10] =
                { { -342.62884098034357, -3483.4445569178347, -9735.250875334352,
                    -10782.158977031822, -5548.704065887224, -1446.951694673217,
                    -200.19589605282445, -14.786385491859248, -0.5489744177844188,
                    -0.008042950896814532 },
                  { 1., -84.8098437770271, -1884.7738197074218, -8059.219012060384,
                    -11916.470977597566, -7349.477378676199, -2013.8039726540235,
                    -237.47722999429413, -9.646075249097724,
                    -2.2132610916769585e-8 } };
        #endif

        size_t ncoeffs =
            to_srgb ? std::extent_v<decltype(to_srgb_coeffs), 1> :
                      std::extent_v<decltype(from_srgb_coeffs), 1>;

        for (size_t i = 0; i < ncoeffs; ++i) {
            for (int j = 0; j < 2; ++j) {
                X86Xmm &v = (j == 0) ? a : b;
                X86Mem coeff = const_(to_srgb ? to_srgb_coeffs[j][i]
                                              : from_srgb_coeffs[j][i]);
                if (i == 0)
                    movs(v, coeff);
                else
                    fmadd213(v, y, coeff);
            }
        }

        divs(a, b);
        cc.bind(low_value);
        muls(a, x);

        return a;
    }

    /// Load a variable from the given structure (or return from the cache if it was already loaded)
    std::pair<Key, Value> load(const Struct* struct_, const X86Gp &input, const std::string &name) {
        Struct::Field field = struct_->field(name);

        Key key { field.name, field.type, field.flags };

        auto it = cache.find(key);
        if (it != cache.end())
            return *it;

        #if MTS_JIT_LOG_ASSEMBLY == 1
            cc.comment(("# Load field \""+ name + "\"").c_str());
       #endif

        Value value;

        uint32_t op;
        if (field.is_signed())
            op = field.size < 4 ? X86Inst::kIdMovsx : X86Inst::kIdMovsxd;
        else
            op = field.size < 4 ? X86Inst::kIdMovzx : X86Inst::kIdMov;
        if (field.size == 8)
            op = X86Inst::kIdMov;

        // Will we need to swap the byte order of the source records?
        bool bswap = struct_->byte_order() == Struct::ByteOrder::BigEndian;

        if (Struct::is_integer(field.type) || field.type == Struct::Type::Float16)
            value.gp = cc.newInt64(field.name.c_str());
        else
            value.xmm = cc.newXmm(field.name.c_str());

        const int32_t offset = (int32_t) field.offset;
        switch (field.type) {
            case Struct::Type::UInt8:
            case Struct::Type::Int8:
                cc.emit(op, value.gp, x86::byte_ptr(input, offset));
                break;

            case Struct::Type::UInt16:
            case Struct::Type::Int16:
                if (bswap) {
                    cc.movzx(value.gp, x86::word_ptr(input, offset));
                    cc.xchg(value.gp.r8Lo(), value.gp.r8Hi());
                    if (field.is_signed())
                        cc.emit(op, value.gp.r64(), value.gp.r16());
                } else {
                    cc.emit(op, value.gp, x86::word_ptr(input, offset));
                }
                break;

            case Struct::Type::UInt32:
            case Struct::Type::Int32:
                if (bswap) {
                    cc.mov(value.gp.r32(), x86::dword_ptr(input, offset));
                    cc.bswap(value.gp.r32());
                    if (field.is_signed())
                        cc.emit(op, value.gp.r64(), value.gp.r32());
                } else {
                    cc.emit(op, value.gp.r32(), x86::dword_ptr(input, offset));
                }
                break;

            case Struct::Type::UInt64:
            case Struct::Type::Int64:
                cc.mov(value.gp, x86::qword_ptr(input, offset));
                if (bswap)
                    cc.bswap(value.gp.r64());
                break;

            case Struct::Type::Float16:
                if (bswap) {
                    cc.movzx(value.gp, x86::word_ptr(input, offset));
                    cc.xchg(value.gp.r8Lo(), value.gp.r8Hi());
                } else {
                    cc.emit(op, value.gp.r16(), x86::word_ptr(input, offset));
                }
                break;

            case Struct::Type::Float32:
                if (bswap) {
                    X86Gp temp = cc.newUInt32();
                    cc.mov(temp.r32(), x86::dword_ptr(input, offset));
                    cc.bswap(temp.r32());
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovd(value.xmm, temp.r32());
                    #else
                        cc.movd(value.xmm, temp.r32());
                    #endif
                } else {
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovss(value.xmm, x86::dword_ptr(input, offset));
                    #else
                        cc.movss(value.xmm, x86::dword_ptr(input, offset));
                    #endif
                }
                break;

            case Struct::Type::Float64:
                if (bswap) {
                    X86Gp temp = cc.newUInt64();
                    cc.mov(temp.r64(), x86::qword_ptr(input, offset));
                    cc.bswap(temp.r64());
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovq(value.xmm, temp.r64());
                    #else
                        cc.movq(value.xmm, temp.r64());
                    #endif
                } else {
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovsd(value.xmm, x86::qword_ptr(input, offset));
                    #else
                        cc.movsd(value.xmm, x86::qword_ptr(input, offset));
                    #endif
                }
                break;

            default: Throw("StructConverter: unknown field type!");
        }

        if (has_flag(field.flags, Struct::Flags::Assert)) {
            if (field.type == Struct::Type::Float16) {
                auto ref = cc.newUInt16Const(
                    asmjit::kConstScopeGlobal,
                    enoki::half::float32_to_float16((float) field.default_));
                cc.cmp(value.gp.r16(), ref);
            } else if (field.type == Struct::Type::Float32) {
                auto ref = cc.newFloatConst(asmjit::kConstScopeGlobal, (float) field.default_);
                #if defined(ENOKI_X86_AVX)
                    cc.vucomiss(value.xmm, ref);
                #else
                    cc.ucomiss(value.xmm, ref);
                #endif
            } else if (field.type == Struct::Type::Float64) {
                auto ref = cc.newDoubleConst(asmjit::kConstScopeGlobal, (double) field.default_);
                #if defined(ENOKI_X86_AVX)
                    cc.vucomisd(value.xmm, ref);
                #else
                    cc.ucomisd(value.xmm, ref);
                #endif
            } else if (field.type == Struct::Type::Int8 || field.type == Struct::Type::UInt8) {
                auto ref = cc.newByteConst(asmjit::kConstScopeGlobal, (int8_t) field.default_);
                cc.cmp(value.gp.r8(), ref);
            } else if (field.type == Struct::Type::Int16 || field.type == Struct::Type::UInt16) {
                auto ref = cc.newInt16Const(asmjit::kConstScopeGlobal, (int16_t) field.default_);
                cc.cmp(value.gp.r16(), ref);
            } else if (field.type == Struct::Type::Int32 || field.type == Struct::Type::UInt32) {
                auto ref = cc.newInt32Const(asmjit::kConstScopeGlobal, (int32_t) field.default_);
                cc.cmp(value.gp.r32(), ref);
            } else if (field.type == Struct::Type::Int64 || field.type == Struct::Type::UInt64) {
                auto ref = cc.newInt64Const(asmjit::kConstScopeGlobal, (int64_t) field.default_);
                cc.cmp(value.gp.r64(), ref);
            } else {
                Throw("Internal error!");
            }
            cc.jne(err_label);
        }

        cache[key] = value;

        return std::make_pair(key, value);
    }

    std::pair<Key, Value> load_default(const Struct::Field &field) {
        Key key { field.name, struct_type_v<Float>, +Struct::Flags::None };
        Value value;
        value.xmm = cc.newXmm();
        movs(value.xmm, const_(field.default_));
        return std::make_pair(key, value);
    }

    /// Convert a variable into a linear floating point representation
    std::pair<Key, Value> linearize(const std::pair<Key, Value> &input) {
        Key kr = input.first;
        Value vr = input.second;

        bool int_to_float = false;
        bool float_to_float = false;
        bool inv_gamma = false;

        if (Struct::is_integer(kr.type)) {
            kr.type = struct_type_v<Float>;
            kr.flags &= ~Struct::Flags::Normalized;
            int_to_float = true;
        }

        if (Struct::is_float(kr.type) && kr.type != struct_type_v<Float>) {
            kr.type = struct_type_v<Float>;
            float_to_float = true;
        }

        if (has_flag(kr.flags, Struct::Flags::Gamma)) {
            kr.flags &= ~Struct::Flags::Gamma;
            inv_gamma = true;
        }

        auto it = cache.find(kr);
        if (it != cache.end())
            return *it;

        if (int_to_float) {
            auto range = Struct::range(input.first.type);

            vr.xmm = cc.newXmm();

            if (input.first.type == Struct::Type::UInt32 || input.first.type == Struct::Type::Int64) {
                cvtsi2s(vr.xmm, vr.gp.r64());
            } else if (input.first.type == Struct::Type::UInt64) {
                auto tmp = cc.newUInt64();
                cc.mov(tmp, vr.gp.r64());
                auto tmp2 = cc.newUInt64Const(asmjit::kConstScopeGlobal, 0x7fffffffffffffffull);
                cc.and_(tmp, tmp2);
                cvtsi2s(vr.xmm, tmp.r64());
                cc.test(vr.gp.r64(), vr.gp.r64());
                Label done = cc.newLabel();
                cc.jns(done);
                adds(vr.xmm, const_(0x8000000000000000ull));
                cc.bind(done);
            } else {
                cvtsi2s(vr.xmm, vr.gp.r32());
            }

            if (has_flag(input.first.flags, Struct::Flags::Normalized))
                muls(vr.xmm, const_(1.0 / range.second));
        }

        if (float_to_float) {
            kr.type = input.first.type;

            if (kr.type == Struct::Type::Float16) {
                vr.xmm = cc.newXmm();
                #if defined(ENOKI_X86_AVX) && defined(ENOKI_X86_F16C)
                    cc.vmovd(vr.xmm, vr.gp.r32());
                    cc.vcvtph2ps(vr.xmm, vr.xmm);
                #else
                    auto call = cc.call(imm_ptr((void *) enoki::half::float16_to_float32),
                        FuncSignature1<float, uint16_t>(CallConv::kIdHostCDecl));
                    call->setArg(0, vr.gp);
                    call->setRet(0, vr.xmm);
                #endif
                kr.type = Struct::Type::Float32;
            }

            if (kr.type == Struct::Type::Float32 && kr.type != struct_type_v<Float>) {
                X86Xmm source = vr.xmm;
                vr.xmm = cc.newXmm();
                #if defined(ENOKI_X86_AVX)
                    cc.vcvtss2sd(vr.xmm, vr.xmm, source);
                #else
                    cc.cvtss2sd(vr.xmm, source);
                #endif
                kr.type = Struct::Type::Float64;
            }

            if (kr.type == Struct::Type::Float64 && kr.type != struct_type_v<Float>) {
                X86Xmm source = vr.xmm;
                vr.xmm = cc.newXmm();
                #if defined(ENOKI_X86_AVX)
                    cc.vcvtsd2ss(vr.xmm, vr.xmm, source);
                #else
                    cc.cvtsd2ss(vr.xmm, source);
                #endif
                kr.type = Struct::Type::Float32;
            }
        }

        if (inv_gamma)
            vr.xmm = gamma(vr.xmm, false);

        cache[kr] = vr;

        return std::make_pair(kr, vr);
    }

    /// Write a variable to memory
    void save(const Struct *struct_, const X86Gp &output,
              Struct::Field field, const std::pair<Key, Value> &kv) {
        Key key = kv.first; Value value = kv.second;

        #if MTS_JIT_LOG_ASSEMBLY == 1
            cc.comment(("# Save field \""+ field.name + "\"").c_str());
        #endif

        if (has_flag(field.flags, Struct::Flags::Gamma) &&
           !has_flag(key.flags,   Struct::Flags::Gamma)) {
            value.xmm = gamma(value.xmm, true);
        }

        if (field.is_integer() && !Struct::is_integer(key.type)) {
            auto range_dbl = field.range();
            std::pair<Float, Float> range = range_dbl;
            if (key.type != struct_type_v<Float>)
                Throw("Internal error!");

            auto temp = cc.newXmm();
            movs(temp, value.xmm);
            value.xmm = temp;

            while ((double) range.first < range_dbl.first)
                range.first = enoki::next_float(range.first);
            while ((double) range.second > range_dbl.second)
                range.second = enoki::prev_float(range.second);

            if (has_flag(field.flags, Struct::Flags::Normalized)) {
                muls(value.xmm, const_(range.second));

                if (dither) {
                    if (!dither_ready) {
                        X86Gp index = cc.newUInt64();
                        cc.movzx(index.r64(), xp.r8Lo());
                        cc.mov(index.r8Hi(), yp.r8Lo());
                        X86Gp base = cc.newUInt64();
                        cc.mov(base.r64(), Imm((uintptr_t) dither_matrix256));
                        dither_value = cc.newXmm();
                        #if defined(ENOKI_X86_AVX)
                            cc.movss(dither_value, X86Mem(base, index, 2, 0, (uint32_t) sizeof(float)));
                        #else
                            cc.vmovss(dither_value, X86Mem(base, index, 2, 0, (uint32_t) sizeof(float)));
                        #endif
                        #if defined(DOUBLE_PRECISION)
                            #if defined(ENOKI_X86_AVX)
                                cc.vcvtss2sd(dither_value, dither_value, dither_value);
                            #else
                                cc.cvtss2sd(dither_value, dither_value);
                            #endif
                        #endif
                        dither_ready = true;
                    }
                    adds(value.xmm, dither_value);
                }
            }

            rounds(value.xmm, value.xmm, 8);
            maxs(value.xmm, const_(range.first));
            mins(value.xmm, const_(range.second));
            value.gp = cc.newUInt64();

            if (field.type == Struct::Type::UInt32 || field.type == Struct::Type::Int64) {
                cvts2si(value.gp.r64(), value.xmm);
            } else if (field.type == Struct::Type::UInt64) {
                cvts2si(value.gp.r64(), value.xmm);

                X86Xmm large_thresh = cc.newXmm();
                movs(large_thresh, const_(9.223372036854776e18 /* 2^63 - 1 */));

                X86Xmm tmp = cc.newXmm();
                subs(tmp, value.xmm, large_thresh);

                X86Gp tmp2 = cc.newInt64();
                cvts2si(tmp2, tmp);

                X86Gp large_result = cc.newInt64();
                cc.mov(large_result, Imm(0x7fffffffffffffffull));
                cc.add(large_result, tmp2);

                ucomis(value.xmm, large_thresh);
                cc.cmovnb(value.gp.r64(), large_result);
            } else {
                cvts2si(value.gp.r32(), value.xmm);
            }
        }

        bool bswap = struct_->byte_order() == Struct::ByteOrder::BigEndian;
        const int32_t offset = (int32_t) field.offset;

        switch (field.type) {
            case Struct::Type::UInt8:
            case Struct::Type::Int8:
                cc.mov(x86::byte_ptr(output, offset), value.gp.r8());
                break;

            case Struct::Type::Int16:
            case Struct::Type::UInt16:
                if (bswap) {
                   X86Gp temp = cc.newUInt16();
                   cc.mov(temp, value.gp.r16());
                   cc.xchg(temp.r8Lo(), temp.r8Hi());
                   value.gp = temp;
                }
                cc.mov(x86::word_ptr(output, offset), value.gp.r16());
                break;

            case Struct::Type::Int32:
            case Struct::Type::UInt32:
                if (bswap) {
                   X86Gp temp = cc.newUInt32();
                   cc.mov(temp, value.gp.r32());
                   cc.bswap(temp);
                   value.gp = temp;
                }
                cc.mov(x86::dword_ptr(output, offset), value.gp.r32());
                break;

            case Struct::Type::Int64:
            case Struct::Type::UInt64:
                if (bswap) {
                   X86Gp temp = cc.newUInt64();
                   cc.mov(temp, value.gp.r64());
                   cc.bswap(temp);
                   value.gp = temp;
                }
                cc.mov(x86::qword_ptr(output, offset), value.gp.r64());
                break;

            case Struct::Type::Float16:
                if (key.type == Struct::Type::Float64) {
                    X86Xmm temp = cc.newXmm();
                    #if defined(ENOKI_X86_AVX)
                        cc.vcvtsd2ss(temp, temp, value.xmm);
                    #else
                        cc.cvtsd2ss(temp, value.xmm);
                    #endif
                    value.xmm = temp;
                    key.type = Struct::Type::Float32;
                }
                if (key.type == Struct::Type::Float32) {
                    value.gp = cc.newUInt32();

                    #if defined(__F16C__)
                        X86Xmm temp = cc.newXmm();
                        cc.vcvtps2ph(temp, value.xmm, 0);
                        cc.vmovd(value.gp.r32(), temp);
                    #else
                        auto call = cc.call(imm_ptr((void *) enoki::half::float32_to_float16),
                            FuncSignature1<uint16_t, float>(asmjit::CallConv::kIdHost));
                        call->setArg(0, value.xmm);
                        call->setRet(0, value.gp);
                    #endif

                    key.type = Struct::Type::Float16;
                }

                if (bswap) {
                   X86Gp temp = cc.newUInt16();
                   cc.mov(temp, value.gp.r16());
                   cc.xchg(temp.r8Lo(), temp.r8Hi());
                   value.gp = temp;
                }

                cc.mov(x86::word_ptr(output, offset), value.gp.r16());

                break;

            case Struct::Type::Float32:
                if (key.type == Struct::Type::Float64) {
                    X86Xmm temp = cc.newXmm();
                    #if defined(ENOKI_X86_AVX)
                        cc.vcvtsd2ss(temp, temp, value.xmm);
                    #else
                        cc.cvtsd2ss(temp, value.xmm);
                    #endif
                    value.xmm = temp;
                }
                if (bswap) {
                    X86Gp temp = cc.newUInt32();
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovd(temp, value.xmm);
                    #else
                        cc.movd(temp, value.xmm);
                    #endif
                    cc.bswap(temp);
                    cc.mov(x86::dword_ptr(output, offset), temp);
                } else {
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovss(x86::dword_ptr(output, offset), value.xmm);
                    #else
                        cc.movss(x86::dword_ptr(output, offset), value.xmm);
                    #endif
                }
                break;

            case Struct::Type::Float64:
                if (key.type == Struct::Type::Float32) {
                    X86Xmm temp = cc.newXmm();
                    #if defined(ENOKI_X86_AVX)
                        cc.vcvtss2sd(temp, temp, value.xmm);
                    #else
                        cc.cvtss2sd(temp, value.xmm);
                    #endif
                    value.xmm = temp;
                }
                if (bswap) {
                    X86Gp temp = cc.newUInt64();
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovq(temp, value.xmm);
                    #else
                        cc.movq(temp, value.xmm);
                    #endif
                    cc.bswap(temp);
                    cc.mov(x86::qword_ptr(output, offset), temp);
                } else {
                    #if defined(ENOKI_X86_AVX)
                        cc.vmovsd(x86::qword_ptr(output, offset), value.xmm);
                    #else
                        cc.movsd(x86::qword_ptr(output, offset), value.xmm);
                    #endif
                }
                break;

            default: Throw("StructConverter: unknown field type!");
        }
    }
private:
    // Cache of all currently loaded/converted variables
    X86Compiler &cc;
    X86Gp xp, yp;
    bool dither;
    Label err_label;
    X86Xmm dither_value;
    bool dither_ready = false;
    std::map<Key, Value> cache;
};

#endif

NAMESPACE_END(detail)

Struct::Struct(bool pack, Struct::ByteOrder byte_order)
    : Object(), m_pack(pack), m_byte_order(byte_order) {
    if (m_byte_order == Struct::ByteOrder::HostByteOrder)
        m_byte_order = host_byte_order();
}

Struct::Struct(const Struct &s)
    : Object(), m_fields(s.m_fields), m_pack(s.m_pack),
      m_byte_order(s.m_byte_order) { }

size_t Struct::size() const {
    if (m_fields.empty())
        return 0;
    auto const &last = m_fields[m_fields.size() - 1];
    size_t size = last.offset + last.size;
    if (!m_pack) {
        size_t align = alignment();
        size += math::modulo(align - size, align);
    }
    return size;
}

size_t Struct::alignment() const {
    if (m_pack)
        return 1;
    size_t size = 1;
    for (auto const &field : m_fields)
        size = std::max(size, field.size);
    return size;
}

bool Struct::has_field(const std::string &name) const {
    for (auto const &field : m_fields)
        if (field.name == name)
            return true;
    return false;
}

Struct &Struct::append(const std::string &name, Struct::Type type, uint32_t flags, double default_) {
    Field f;
    f.name = name;
    f.type = type;
    f.flags = flags;
    f.default_ = default_;
    if (m_fields.empty()) {
        f.offset = 0;
    } else {
        auto const &last = m_fields[m_fields.size() - 1];
        f.offset = last.offset + last.size;
    }
    switch (type) {
        case Type::Int8:
        case Type::UInt8:   f.size = 1; break;
        case Type::Int16:
        case Type::UInt16:
        case Type::Float16: f.size = 2; break;
        case Type::Int32:
        case Type::UInt32:
        case Type::Float32: f.size = 4; break;
        case Type::Int64:
        case Type::UInt64:
        case Type::Float64: f.size = 8; break;
        default: Throw("Struct::append(): invalid field type!");
    }
    if (!m_pack)
        f.offset += math::modulo(f.size - f.offset, f.size);
    m_fields.push_back(f);
    return *this;
}

std::ostream &operator<<(std::ostream &os, Struct::Type value) {
    switch (value) {
        case Struct::Type::Int8:    os << "int8";    break;
        case Struct::Type::UInt8:   os << "uint8";   break;
        case Struct::Type::Int16:   os << "int16";   break;
        case Struct::Type::UInt16:  os << "uint16";  break;
        case Struct::Type::Int32:   os << "int32";   break;
        case Struct::Type::UInt32:  os << "uint32";  break;
        case Struct::Type::Int64:   os << "int64";   break;
        case Struct::Type::UInt64:  os << "uint64";  break;
        case Struct::Type::Float16: os << "float16"; break;
        case Struct::Type::Float32: os << "float32"; break;
        case Struct::Type::Float64: os << "float64"; break;
        case Struct::Type::Invalid: os << "invalid"; break;
        default: Throw("Struct: operator<<: invalid field type!");
    }
    return os;
}

std::string Struct::to_string() const {
    std::ostringstream os;
    os << "Struct<" << size() << ">[" << std::endl;
    for (size_t i = 0; i < m_fields.size(); ++i) {
        auto const &f = m_fields[i];
        if (i > 0) {
            size_t padding = f.offset - (m_fields[i-1].offset + m_fields[i-1].size);
            if (padding > 0)
                os << "  // " << padding << " byte" << (padding > 1 ? "s" : "") << " of padding." << std::endl;
        }
        os << "  " << f.type;
        os << " " << f.name << "; // @" << f.offset;
        if (has_flag(f.flags, Flags::Normalized))
            os << ", normalized";
        if (has_flag(f.flags, Flags::Gamma))
            os << ", gamma";
        if (has_flag(f.flags, Flags::Weight))
            os << ", weight";
        if (has_flag(f.flags, Flags::Alpha))
            os << ", alpha";
        if (has_flag(f.flags, Flags::PremultipliedAlpha))
            os << ", premultiplied alpha";
        if (has_flag(f.flags, Flags::Default))
            os << ", default=" << f.default_;
        if (has_flag(f.flags, Flags::Assert))
            os << ", assert=" << f.default_;
        if (!f.blend.empty()) {
            os << ", blend = <";
            for (size_t j = 0; j < f.blend.size(); ++j) {
                os << f.blend[j].second << " * " << f.blend[j].first;
                if (j + 1 < f.blend.size())
                    os << " + ";
            }
            os << ">";
        }
        os << "\n";
    }
    if (!m_fields.empty()) {
        size_t padding = size() - (m_fields[m_fields.size() - 1].offset +
                                   m_fields[m_fields.size() - 1].size);
        if (padding > 0)
            os << "  // " << padding << " byte" << (padding > 1 ? "s" : "") << " of padding." << std::endl;
    }
    os << "]";
    return os.str();
}

const Struct::Field &Struct::field(const std::string &name) const {
    for (auto const &field : m_fields)
        if (field.name == name)
            return field;
    Throw("Unable to find field \"%s\"", name);
}

Struct::Field &Struct::field(const std::string &name) {
    for (auto &field : m_fields)
        if (field.name == name)
            return field;
    Throw("Unable to find field \"%s\"", name);
}

std::pair<double, double> Struct::range(Struct::Type type) {
    std::pair<double, double> result;

    #define COMPUTE_RANGE(key, type)                                        \
        case key:                                                           \
            result = std::make_pair(std::numeric_limits<type>::min(),       \
                                    std::numeric_limits<type>::max());      \
            break;

    switch (type) {
        COMPUTE_RANGE(Type::UInt8, uint8_t);
        COMPUTE_RANGE(Type::Int8, int8_t);
        COMPUTE_RANGE(Type::UInt16, uint16_t);
        COMPUTE_RANGE(Type::Int16, int16_t);
        COMPUTE_RANGE(Type::UInt32, uint32_t);
        COMPUTE_RANGE(Type::Int32, int32_t);
        COMPUTE_RANGE(Type::UInt64, uint64_t);
        COMPUTE_RANGE(Type::Int64, int64_t);
        COMPUTE_RANGE(Type::Float32, float);
        COMPUTE_RANGE(Type::Float64, double);

        case Type::Float16:
            result = std::make_pair(-65504, 65504);
            break;

        default:
            Throw("Internal error: invalid field type");
    }

    if (is_integer(type)) {
        // Account for rounding errors in the conversions above.
        // (we want the bounds to be conservative)
        if (result.first != 0)
            result.first = enoki::next_float(result.first);
        result.second = enoki::prev_float(result.second);
    }

    return result;
}

size_t hash(const Struct::Field &f) {
    size_t value = hash(f.name);
    value = hash_combine(value, hash(f.type));
    value = hash_combine(value, hash(f.size));
    value = hash_combine(value, hash(f.offset));
    value = hash_combine(value, hash(f.flags));
    value = hash_combine(value, hash(f.default_));
    return value;
}

size_t hash(const Struct &s) {
    return hash_combine(hash_combine(hash(s.m_fields), hash(s.m_pack)),
                        hash(s.m_byte_order));
}

static std::unordered_map<
           std::pair<ref<const Struct>, ref<const Struct>>, void *,
    hasher<std::pair<ref<const Struct>, ref<const Struct>>>,
    comparator<std::pair<ref<const Struct>, ref<const Struct>>>> __cache;

StructConverter::StructConverter(const Struct *source, const Struct *target, bool dither)
 : m_source(source), m_target(target) {
#if MTS_STRUCTCONVERTER_USE_JIT == 1
    using namespace asmjit;

    // Use the Jit instance to cache structure converters
    auto jit = Jit::get_instance();
    std::lock_guard<std::mutex> guard(jit->mutex);

    auto key = std::make_pair(ref<const Struct>(source), ref<const Struct>(target));
    auto it = __cache.find(key);

    if (it != __cache.end()) {
        // Cache hit
        m_func = ptr_as_func<FuncType>(it->second);
        return;
    }

    CodeHolder code;
    code.init(jit->runtime.getCodeInfo());
    #if MTS_JIT_LOG_ASSEMBLY == 1
        Log(Info, "Converting from %s to %s", source, target);
        StringLogger logger;
        logger.addOptions(asmjit::Logger::kOptionBinaryForm);
        code.setLogger(&logger);
    #endif

    X86Compiler cc(&code);

    cc.addFunc(FuncSignature4<bool, size_t, size_t, const void *, void *>(asmjit::CallConv::kIdHost));
    auto width = cc.newInt64("width");
    auto height = cc.newInt64("height");
    auto input = cc.newIntPtr("input");
    auto output = cc.newIntPtr("output");
    auto x = cc.newUInt64("x");
    auto y = cc.newUInt64("y");

    cc.setArg(0, width);
    cc.setArg(1, height);
    cc.setArg(2, input);
    cc.setArg(3, output);

    // Control flow structure
    Label loop_start = cc.newLabel();
    Label loop_x_end = cc.newLabel();
    Label loop_y_end = cc.newLabel();
    Label loop_fail  = cc.newLabel();

    detail::StructCompiler sc(cc, x, y, dither, loop_fail);

    cc.test(width, width);
    cc.jz(loop_y_end);
    cc.xor_(x, x);

    cc.test(height, height);
    cc.jz(loop_y_end);
    cc.xor_(y, y);

    cc.bind(loop_start);

    bool has_assert = false;
    // Ensure that fields with an EAssert flag are loaded
    for (const Struct::Field &f : *source) {
        if (has_flag(f.flags, Struct::Flags::Assert)) {
            sc.load(source, input, f.name);
            has_assert = true;
        }
    }

    const Struct::Field *source_weight = nullptr;
    const Struct::Field *target_weight = nullptr;

    for (const Struct::Field &f : *source) {
        if (!has_flag(f.flags, Struct::Flags::Weight))
            continue;
        if (source_weight != nullptr)
            Throw("Internal error: source structure has more than one weight field!");
        source_weight = &f;
    }

    for (const Struct::Field &f : *target) {
        if (!has_flag(f.flags, Struct::Flags::Weight))
            continue;
        if (target_weight != nullptr)
            Throw("Internal error: target structure has more than one weight field!");
        target_weight = &f;
    }

    if (source_weight != nullptr && target_weight != nullptr) {
        if (source_weight->name != target_weight->name)
            Throw("Internal error: source and target weights have mismatched names!");
    }

    X86Xmm scale_factor;
    if (source_weight != nullptr && target_weight == nullptr) {
        scale_factor = cc.newXmm();
        X86Xmm value = sc.linearize(sc.load(source, input, source_weight->name)).second.xmm;
        sc.movs(scale_factor, sc.const_(1.0));
        sc.divs(scale_factor, value);
    }

    const Struct::Field *source_alpha = nullptr;
    const Struct::Field *target_alpha = nullptr;
    bool has_multiple_alpha_channels = false;
    for (const Struct::Field &f : *source) {
        if (!has_flag(f.flags, Struct::Flags::Alpha))
            continue;
        if (source_alpha != nullptr) {
            has_multiple_alpha_channels = true;
            break;
        }
        source_alpha = &f;
    }

    for (const Struct::Field &f : *target) {
        if (!has_flag(f.flags, Struct::Flags::Alpha))
            continue;
        target_alpha = &f;
        break;
    }

    if (source_alpha != nullptr && target_alpha != nullptr) {
        if (source_alpha->name != target_alpha->name)
            Throw("Internal error: source and target alpha have mismatched names!");
    }

    X86Xmm alpha, inv_alpha;
    if (source_alpha != nullptr && target_alpha != nullptr) {
        alpha = cc.newXmm();
        inv_alpha = cc.newXmm();
        X86Xmm value = sc.linearize(sc.load(source, input, source_alpha->name)).second.xmm;
        sc.movs(alpha, value);
        sc.movs(inv_alpha, sc.const_(1.0));
        sc.divs(inv_alpha, value);

        // Check if alpha is zero and set inv_alpha to zero if that is the case
        X86Xmm zero = cc.newXmm();
        sc.movs(zero, sc.const_(0.0));

        X86Xmm mask = cc.newXmm();
        sc.movs(mask, value);
        sc.cmps(mask, zero, 2);
        sc.blend(inv_alpha, zero, mask);
    }


    for (const Struct::Field &f : *target) {
        std::pair<detail::StructCompiler::Key, detail::StructCompiler::Value> kv;
        if (f.blend.empty()) {
            if (source->has_field(f.name)) {
                kv = sc.load(source, input, f.name);
            } else if (has_flag(f.flags, Struct::Flags::Default)) {
                kv = sc.load_default(f);
            } else {
                Throw("Unable to find field \"%s\"!", f.name);
            }
        } else {
            X86Xmm accum = cc.newXmm();
            for (size_t i = 0; i<f.blend.size(); ++i) {
                kv = sc.linearize(sc.load(source, input, f.blend[i].second));
                if (i == 0)
                    sc.muls(accum, kv.second.xmm, sc.const_(f.blend[i].first));
                else
                    sc.fmadd231(accum, kv.second.xmm, sc.const_(f.blend[i].first));
            }
            kv.first.name = "_" + f.name + "_blend";
            kv.second.xmm = accum;
        }

        uint32_t flag_mask = Struct::Flags::Normalized | Struct::Flags::Gamma;
        if (!((kv.first.type == f.type || (Struct::is_integer(kv.first.type) &&
                                           Struct::is_integer(f.type) &&
                                           !has_flag(f.flags, Struct::Flags::Normalized))) &&
            ((kv.first.flags & flag_mask) == (f.flags & flag_mask))))
            kv = sc.linearize(kv);

        if (source_weight != nullptr && target_weight == nullptr) {
            X86Xmm result = cc.newXmm();
            if (kv.first.type != struct_type_v<Float>)
                kv = sc.linearize(kv);
            sc.muls(result, kv.second.xmm, scale_factor);
            kv.second.xmm = result;
        }

        uint32_t special_channels_mask = Struct::Flags::Weight | Struct::Flags::Alpha;
        bool source_premult = has_flag(kv.first.flags, Struct::Flags::PremultipliedAlpha);
        bool target_premult = has_flag(f.flags, Struct::Flags::PremultipliedAlpha);

        if (source_alpha != nullptr && target_alpha != nullptr && ((f.flags & special_channels_mask) == 0) &&
            source_premult != target_premult) {
            if (has_multiple_alpha_channels)
                Throw("Found multiple alpha channels: Alpha (un)premultiplication expects a single alpha channel");
            X86Xmm result = cc.newXmm();
            if (kv.first.type != struct_type_v<Float>)
                kv = sc.linearize(kv);
            if (target_premult && !source_premult) {
                sc.muls(result, kv.second.xmm, alpha);
                kv.second.xmm = result;
            } else if (!target_premult && source_premult) {
                sc.muls(result, kv.second.xmm, inv_alpha);
                kv.second.xmm = result;
            }
        }
        sc.save(target, output, f, kv);
    }

    cc.inc(x);
    cc.add(input,  Imm(source->size()));
    cc.add(output, Imm(target->size()));
    cc.cmp(x, width);
    cc.jne(loop_start);

    cc.bind(loop_x_end);
    cc.xor_(x, x);
    cc.inc(y);
    cc.cmp(y, height);
    cc.jne(loop_start);

    cc.bind(loop_y_end);
    auto rv = cc.newInt64("rv");
    cc.mov(rv.r32(), Imm(1));
    cc.ret(rv);
    if (has_assert) {
        cc.bind(loop_fail);
        cc.xor_(rv, rv);
        cc.ret(rv);
    }
    cc.endFunc();

    asmjit::Error err = cc.finalize();
    if (err != asmjit::kErrorOk)
        Throw("asmjit failed: %s", asmjit::DebugUtils::errorAsString(err));

    jit->runtime.add((void **) &m_func, &code);

    #if MTS_JIT_LOG_ASSEMBLY == 1
       Log(Info, "Assembly:\n%s", logger.getString());
    #endif

    __cache[key] = (void *) m_func;
#else
    m_dither = dither;
#endif
}

#if MTS_STRUCTCONVERTER_USE_JIT == 0

bool StructConverter::load(const uint8_t *src, const Struct::Field &f, Value &value) const {
    bool source_swap = m_source->byte_order() != Struct::host_byte_order();

    src += f.offset;
    value.type = f.type;
    value.flags = f.flags;

    switch (f.type) {
        case Struct::Type::UInt8:
            value.u = *((const uint8_t *) src);
            break;

        case Struct::Type::Int8:
            value.i = *((const int8_t *) src);
            break;

        case Struct::Type::UInt16: {
                uint16_t val = *((const uint16_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.u = val;
            }
            break;

        case Struct::Type::Int16: {
                int16_t val = *((const int16_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.i = val;
            }
            break;

        case Struct::Type::UInt32: {
                uint32_t val = *((const uint32_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.u =  val;
            }
            break;

        case Struct::Type::Int32: {
                int32_t val = *((const int32_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.i = val;
            }
            break;

        case Struct::Type::UInt64: {
                uint64_t val = *((const uint64_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.u = val;
            }
            break;

        case Struct::Type::Int64: {
                int64_t val = *((const int64_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.i = val;
            }
            break;

        case Struct::Type::Float16: {
                uint16_t val = *((const uint16_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.s = enoki::half::float16_to_float32(val);
                value.type = Struct::Type::Float32;
            }
            break;

        case Struct::Type::Float32: {
                uint32_t val = *((const uint32_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.s = memcpy_cast<float>(val);
            }
            break;

        case Struct::Type::Float64: {
                uint64_t val = *((const uint64_t *) src);
                if (source_swap)
                    val = detail::swap(val);
                value.d = memcpy_cast<double>(val);
            }
            break;

        default: Throw("StructConverter: unknown field type!");
    }

    if (has_flag(f.flags, Struct::Flags::Assert)) {
        if (f.is_integer()) {
            if (f.is_signed() && (int64_t) f.default_ != value.i)
                return false;
            if (!f.is_signed() && (uint64_t) f.default_ != value.u)
                return false;

        }
        if (value.type == Struct::Type::Float32 && (Float) f.default_ != value.s)
            return false;
        if (value.type == Struct::Type::Float64 && f.default_ != value.d)
            return false;
    }
    return true;
}

void StructConverter::linearize(Value &value) const {

    if (Struct::is_integer(value.type)) {
        if (Struct::is_unsigned(value.type))
            value.f = (Float) value.u;
        else
            value.f = (Float) value.i;

        if (has_flag(value.flags, Struct::Flags::Normalized))
            value.f *= Float(1 / Struct::range(value.type).second);
    } else if (Struct::is_float(value.type) && value.type != struct_type_v<Float>) {
        if (value.type == Struct::Type::Float32)
            value.f = (Float) value.s;
        else
            value.f = (Float) value.d;
    }
    if (has_flag(value.flags, Struct::Flags::Gamma)) {
        value.f = srgb_to_linear(value.f);
        value.flags = value.flags & ~Struct::Flags::Gamma;
    }

    value.type = struct_type_v<Float>;
}

void StructConverter::save(uint8_t *dst, const Struct::Field &f, Value value, size_t x, size_t y) const {

    /* Is swapping needed */
    bool target_swap = m_target->byte_order() != Struct::host_byte_order();

    dst += f.offset;

    if (has_flag(f.flags, Struct::Flags::Gamma) && !has_flag(value.flags, Struct::Flags::Gamma))
        value.f = enoki::linear_to_srgb(value.f);

    if (f.is_integer() && value.type == struct_type_v<Float>) {
        auto range = f.range();

        if (has_flag(f.flags, Struct::Flags::Normalized))
            value.f *= (Float) range.second;

        double d = (double) value.f;

        if (m_dither)
            d += (double) dither_matrix256[(y % 256)*256 + (x%256)];

        d = std::max(d, range.first);
        d = std::min(d, range.second);
        d = std::rint(d);

        if (Struct::is_signed(f.type))
            value.i = (int64_t) d;
        else
            value.u = (uint64_t) d;
    }

    if ((f.type == Struct::Type::Float16 || f.type == Struct::Type::Float32) && value.type == struct_type_v<Float>)
        value.s = (float) value.f;
    else if (f.type == Struct::Type::Float64 && value.type == struct_type_v<Float>)
        value.d = (double) value.f;

    switch (f.type) {
        case Struct::Type::UInt8:
            *((uint8_t *) dst) = (uint8_t) value.i;
            break;

        case Struct::Type::Int8:
            *((int8_t *) dst) = (int8_t) value.u;
            break;

        case Struct::Type::UInt16: {
                uint16_t val = (uint16_t) value.u;
                if (target_swap)
                    val = detail::swap(val);
                *((uint16_t *) dst) = val;
            }
            break;

        case Struct::Type::Int16: {
                int16_t val = (int16_t) value.i;
                if (target_swap)
                    val = detail::swap(val);
                *((int16_t *) dst) = val;
            }
            break;

        case Struct::Type::UInt32: {
                uint32_t val = (uint32_t) value.u;
                if (target_swap)
                    val = detail::swap(val);
                *((uint32_t *) dst) = val;
            }
            break;

        case Struct::Type::Int32: {
                int32_t val = (int32_t) value.i;
                if (target_swap)
                    val = detail::swap(val);
                *((int32_t *) dst) = val;
            }
            break;

        case Struct::Type::UInt64: {
                uint64_t val = (uint64_t) value.u;
                if (target_swap)
                    val = detail::swap(val);
                *((uint64_t *) dst) = val;
            }
            break;

        case Struct::Type::Int64: {
                int64_t val = (int64_t) value.i;
                if (target_swap)
                    val = detail::swap(val);
                *((int64_t *) dst) = val;
            }
            break;

        case Struct::Type::Float16: {
                uint16_t val = enoki::half::float32_to_float16(value.s);
                if (target_swap)
                    val = detail::swap(val);
                *((uint16_t *) dst) = val;
            }
            break;

        case Struct::Type::Float32: {
                uint32_t val = (uint32_t) memcpy_cast<uint32_t>(value.s);
                if (target_swap)
                    val = detail::swap(val);
                *((uint32_t *) dst) = val;
            }
            break;

        case Struct::Type::Float64: {
                uint64_t val = (uint64_t) memcpy_cast<uint64_t>(value.d);
                if (target_swap)
                    val = detail::swap(val);
                *((uint64_t *) dst) = val;
            }
            break;

        default: Throw("StructConverter: unknown field type!");
    }
}

bool StructConverter::convert_2d(size_t width, size_t height, const void *src_, void *dest_) const {
    using namespace mitsuba::detail;

    size_t source_size = m_source->size();
    size_t target_size = m_target->size();
    Struct::Field weight_field, alpha_field;

    bool has_weight = false, has_alpha = false, has_multiple_alpha_channels = false;
    std::vector<Struct::Field> assert_fields;
    for (Struct::Field f : *m_source) {
        if (has_flag(f.flags, Struct::Flags::Assert) && !m_target->has_field(f.name))
            assert_fields.push_back(f);
        if (has_flag(f.flags, Struct::Flags::Weight)) {
            weight_field = f;
            has_weight = true;
        }
        if (has_flag(f.flags, Struct::Flags::Alpha)) {
            has_multiple_alpha_channels |= has_alpha;
            alpha_field = f;
            has_alpha = true;
        }
    }
    for (const Struct::Field &f : *m_target) {
        if (has_flag(f.flags, Struct::Flags::Weight) && has_weight)
            has_weight = false;
    }

    uint8_t *src  = (uint8_t *) src_;
    uint8_t *dest = (uint8_t *) dest_;

    for (size_t y = 0; y<height; ++y) {
        for (size_t x = 0; x<width; ++x) {
            Float inv_weight = 1.f;
            for (const Struct::Field &f : assert_fields) {
                Value value;
                if (!load(src, f, value))
                    return false;
            }

            if (has_weight) {
                Value value;
                if (!load(src, weight_field, value))
                    return false;
                linearize(value);
                inv_weight = 1.f / value.f;
            }
            Float alpha = 1.f, inv_alpha = 1.f;
            if (has_alpha) {
                Value value;
                if (!load(src, alpha_field, value))
                    return false;
                linearize(value);
                alpha = value.f;
                inv_alpha = alpha > 0 ? 1.f / alpha : 0.f;
            }

            for (const Struct::Field &f : *m_target) {
                Value value;

                if (f.blend.empty()) {
                    if (!m_source->has_field(f.name) && has_flag(f.flags, Struct::Flags::Default)) {
                        value.d = f.default_;
                        value.type = Struct::Type::Float64;
                        value.flags = +Struct::Flags::None;
                    } else {
                        if (!load(src, m_source->field(f.name), value))
                            return false;
                    }
                } else {
                    value.type = struct_type_v<Float>;
                    value.f = 0;
                    value.flags = +Struct::Flags::None;
                    for (auto kv : f.blend) {
                        Value value2;
                        if (!load(src, m_source->field(kv.second), value2))
                            return false;
                        linearize(value2);
                        value.f += (Float) kv.first * value2.f;
                    }
                }

                uint32_t flag_mask = Struct::Flags::Normalized | Struct::Flags::Gamma;
                if ((!((value.type == f.type || (Struct::is_integer(value.type) &&
                                                 Struct::is_integer(f.type) &&
                                                 !has_flag(f.flags, Struct::Flags::Normalized))) &&
                    ((value.flags & flag_mask) == (f.flags & flag_mask)))) || has_weight)
                    linearize(value);

                if (has_weight)
                    value.f *= inv_weight;


                uint32_t special_channels_mask = Struct::Flags::Weight | Struct::Flags::Alpha;
                bool source_premult = has_flag(value.flags, Struct::Flags::PremultipliedAlpha);
                bool target_premult = has_flag(f.flags, Struct::Flags::PremultipliedAlpha);
                if (has_alpha && ((f.flags & special_channels_mask) == 0) &&
                    source_premult != target_premult && f.blend.empty()) {
                    linearize(value);
                    if (has_multiple_alpha_channels)
                        Throw("Found multiple alpha channels: Alpha (un)premultiplication expects a single alpha channel");

                    if (target_premult && !source_premult) {
                        value.f *= alpha;
                    } else if (!target_premult && source_premult) {
                        value.f *= inv_alpha;
                    }
                }
                save(dest, f, value, x, y);
            }

            src += source_size;
            dest += target_size;
        }
    }
    return true;
}
#endif

std::string StructConverter::to_string() const {
    std::ostringstream oss;
    oss << "StructConverter[" << std::endl
        << "  source = " << string::indent(m_source) << "," << std::endl
        << "  target = " << string::indent(m_target) << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(Struct, Object)
MTS_IMPLEMENT_CLASS(StructConverter, Object)
NAMESPACE_END(mitsuba)
