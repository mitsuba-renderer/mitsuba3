#include <mitsuba/core/struct.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/hash.h>
#include <mitsuba/core/jit.h>
#include <enoki/array.h>
#include <enoki/half.h>
#include <unordered_map>
#include <ostream>

NAMESPACE_BEGIN(mitsuba)

Struct::Struct(bool pack, EByteOrder byte_order)
    : m_pack(pack), m_byte_order(byte_order) {
    if (m_byte_order == EHostByteOrder)
        m_byte_order = host_byte_order();
}

Struct::Struct(const Struct &s)
    : m_fields(s.m_fields), m_pack(s.m_pack),
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

Struct &Struct::append(const std::string &name, EType type, uint32_t flags, double default_) {
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
        case EInt8:
        case EUInt8:   f.size = 1; break;
        case EInt16:
        case EUInt16:
        case EFloat16: f.size = 2; break;
        case EInt32:
        case EUInt32:
        case EFloat32: f.size = 4; break;
        case EInt64:
        case EUInt64:
        case EFloat64: f.size = 8; break;
        case EFloat:   f.size = sizeof(Float); break;
        default: Throw("Struct::append(): invalid field type!");
    }
    if (!m_pack)
        f.offset += math::modulo(f.size - f.offset, f.size);
    m_fields.push_back(f);
    return *this;
}

std::ostream &operator<<(std::ostream &os, Struct::EType value) {
    switch (value) {
        case Struct::EInt8:    os << "int8";    break;
        case Struct::EUInt8:   os << "uint8";   break;
        case Struct::EInt16:   os << "int16";   break;
        case Struct::EUInt16:  os << "uint16";  break;
        case Struct::EInt32:   os << "int32";   break;
        case Struct::EUInt32:  os << "uint32";  break;
        case Struct::EInt64:   os << "int64";   break;
        case Struct::EUInt64:  os << "uint64";  break;
        case Struct::EFloat16: os << "float16"; break;
        case Struct::EFloat32: os << "float32"; break;
        case Struct::EFloat64: os << "float64"; break;
        case Struct::EFloat:   os << "Float";   break;
        case Struct::EInvalid: os << "invalid"; break;
        default: Throw("Struct: operator<<: invalid field type!");
    }
    return os;
}

std::string Struct::to_string() const {
    std::ostringstream os;
    os << "Struct[" << std::endl;
    for (size_t i = 0; i < m_fields.size(); ++i) {
        auto const &f = m_fields[i];
        os << "  " << f.type;
        os << " " << f.name << "; // @" << f.offset;
        if (f.flags & ENormalized)
            os << ", normalized";
        if (f.flags & EGamma)
            os << ", gamma";
        if (f.flags & EDefault)
            os << ", default=" << f.default_;
        if (f.flags & EAssert)
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

std::pair<double, double> Struct::Field::range() const {
    switch (type) {
        case Struct::EUInt8:
            return std::make_pair(std::numeric_limits<uint8_t>::min(),
                                  std::numeric_limits<uint8_t>::max());
        case Struct::EInt8:
            return std::make_pair(std::numeric_limits<int8_t>::min(),
                                  std::numeric_limits<int8_t>::max());
        case Struct::EUInt16:
            return std::make_pair(std::numeric_limits<uint16_t>::min(),
                                  std::numeric_limits<uint16_t>::max());
        case Struct::EInt16:
            return std::make_pair(std::numeric_limits<int16_t>::min(),
                                  std::numeric_limits<int16_t>::max());
        case Struct::EUInt32:
            return std::make_pair(std::numeric_limits<uint32_t>::min(),
                                  std::numeric_limits<uint32_t>::max());
        case Struct::EInt32:
            return std::make_pair(std::numeric_limits<int32_t>::min(),
                                  std::numeric_limits<int32_t>::max());
        case Struct::EUInt64:
            return std::make_pair(std::numeric_limits<uint64_t>::min(),
                                  std::numeric_limits<uint64_t>::max());
        case Struct::EInt64:
            return std::make_pair(std::numeric_limits<int64_t>::min(),
                                  std::numeric_limits<int64_t>::max());

        case Struct::EFloat16:
            return std::make_pair(-65504, 65504);

        case Struct::EFloat32:
            return std::make_pair(-std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max());

        case Struct::EFloat64:
            return std::make_pair(-std::numeric_limits<double>::max(),
                                   std::numeric_limits<double>::max());

        case Struct::EFloat:
            return std::make_pair(-std::numeric_limits<Float>::max(),
                                   std::numeric_limits<Float>::max());
        default:
            Throw("Internal error: invalid field type");
    }
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

StructConverter::StructConverter(const Struct *source, const Struct *target)
 : m_source(source), m_target(target) {
#if MTS_STRUCTCONVERTER_USE_JIT == 1
    using namespace asmjit;

    auto jit = Jit::getInstance();
    std::lock_guard<std::mutex> guard(jit->mutex);

    auto key = std::make_pair(ref<const Struct>(source), ref<const Struct>(target));
    auto it = __cache.find(key);

    if (it != __cache.end()) {
        m_func = asmjit_cast<FuncType>(it->second);
        return;
    }

    asmjit::X86Assembler assembler(&jit->runtime);

    #if MTS_JIT_LOG_ASSEMBLY == 1
        Log(EInfo, "Converting from %s to %s", source->to_string(), target->to_string());
        StringLogger logger;
        logger.addOptions(asmjit::Logger::kOptionBinaryForm);
        assembler.setLogger(&logger);
    #endif

    asmjit::X86Compiler c(&assembler);

    c.addFunc(FuncBuilder3<void, size_t, const void *, void *>(kCallConvHost));
    auto count = c.newInt64("count");
    auto input = c.newIntPtr("input");
    auto output = c.newIntPtr("output");

    /* Force caller-save registers for these */
    c.alloc(count, x86::r12);
    c.alloc(input, x86::r13);
    c.alloc(output, x86::r14);

    c.setArg(0, count);
    c.setArg(1, input);
    c.setArg(2, output);

    Label loopStart = c.newLabel();
    Label loopEnd = c.newLabel();
    Label loopFail = c.newLabel();

    c.test(count, count);
    c.jz(loopEnd);

    size_t source_size = source->size();

    if (source_size > 1) {
        if (math::is_power_of_two(source_size))
            c.shl(count, Imm(enoki::log2i(source_size)));
        else
            c.imul(count, Imm(source->size()));
    }

    c.add(count, input);
    c.bind(loopStart);

    auto reg    = c.newInt64("reg");
    auto reg_f  = c.newXmmVar(kX86VarTypeXmmSs, "reg_f");
    auto reg_d  = c.newXmmVar(kX86VarTypeXmmSd, "reg_d");
    auto temp_f = c.newXmmVar(kX86VarTypeXmmSs, "temp_f");
    auto temp_d = c.newXmmVar(kX86VarTypeXmmSd, "temp_d");

    bool failNeeded = false;
    std::vector<std::string> fields;
    for (Struct::Field f : *source) {
        if ((f.flags & Struct::EAssert) && !target->has_field(f.name))
            fields.push_back(f.name);
    }
    for (Struct::Field f : *target)
        fields.push_back(f.name);

    for (auto const &name : fields) {
        Struct::Field df;
        if (target->has_field(name))
            df = target->field(name);

        int df_type = df.type;
        if (df_type == Struct::EFloat) {
            if (sizeof(Float) == sizeof(float))
                df_type = Struct::EFloat32;
            else if (sizeof(Float) == sizeof(double))
                df_type = Struct::EFloat64;
            else
                Throw("Invalid 'Float' size");
        }

        try {
            auto sources = df.blend;
            if (sources.empty())
                sources.push_back(std::make_pair(1.0, name));

            Struct::EType sf_type_final = Struct::EInvalid;
            Struct::Field sf;
            Struct::EType &sf_type = sf.type;

            for (size_t i = 0; i< sources.size(); ++i) {
                double weight = sources[i].first;
                sf = source->field(sources[i].second);

                auto reg_f_p = i == 0 ? reg_f : temp_f;
                auto reg_d_p = i == 0 ? reg_d : temp_d;

                bool source_signed = sf.is_signed();
                bool source_swap = source->byte_order() == Struct::EBigEndian;

                uint32_t op;
                if (source_signed) {
                    op = sf.size < 4 ? kX86InstIdMovsx : kX86InstIdMovsxd;
                } else {
                    op = sf.size < 4 ? kX86InstIdMovzx : kX86InstIdMov;
                }
                if (sf.size == 8)
                    op = kX86InstIdMov;

                if (sf_type == Struct::EFloat) {
                    if (sizeof(Float) == sizeof(float))
                        sf_type = Struct::EFloat32;
                    else if (sizeof(Float) == sizeof(double))
                        sf_type = Struct::EFloat64;
                    else
                        Throw("Invalid 'Float' size");
                }

                switch (sf_type) {
                    case Struct::EUInt8:
                    case Struct::EInt8:
                        c.emit(op, reg, x86::byte_ptr(input, (int32_t) sf.offset));
                        break;

                    case Struct::EUInt16:
                    case Struct::EInt16:
                        if (source_swap) {
                            c.movzx(reg, x86::word_ptr(input, (int32_t) sf.offset));
                            c.xchg(reg.r8Lo(), reg.r8Hi());
                            if (source_signed)
                                c.emit(op, reg.r64(), reg.r16());
                        } else {
                            c.emit(op, reg, x86::word_ptr(input, (int32_t) sf.offset));
                        }
                        break;

                    case Struct::EUInt32:
                    case Struct::EInt32:
                        if (source_swap) {
                            c.mov(reg.r32(), x86::dword_ptr(input, (int32_t) sf.offset));
                            c.bswap(reg.r32());
                            if (source_signed)
                                c.emit(op, reg.r64(), reg.r32());
                        } else {
                            c.emit(op, reg, x86::dword_ptr(input, (int32_t) sf.offset));
                        }
                        break;

                    case Struct::EUInt64:
                    case Struct::EInt64:
                        c.mov(reg, x86::qword_ptr(input, (int32_t) sf.offset));
                        if (source_swap)
                            c.bswap(reg.r64());
                        break;

                    case Struct::EFloat16: {
                            c.movzx(reg, x86::word_ptr(input, (int32_t) sf.offset));
                            if (source_swap)
                                c.xchg(reg.r8Lo(), reg.r8Hi());

                            #if defined(__F16C__)
                                c.movd(reg_f_p, reg.r32());
                                c.vcvtph2ps(reg_f_p, reg_f_p);
                            #else
                                auto call = c.call(imm_ptr((void *) enoki::half::float16_to_float32),
                                    FuncBuilder1<float, uint16_t>(kCallConvHost));

                                call->setArg(0, reg);
                                call->setRet(0, reg_f_p);
                            #endif
                            sf_type = Struct::EFloat32;
                        }
                        break;

                    case Struct::EFloat32:
                        if (source_swap) {
                            c.mov(reg.r32(), x86::dword_ptr(input, (int32_t) sf.offset));
                            c.bswap(reg.r32());
                            c.movd(reg_f_p, reg.r32());
                        } else {
                            c.movss(reg_f_p, x86::dword_ptr(input, (int32_t) sf.offset));
                        }
                        break;

                    case Struct::EFloat64:
                        if (source_swap) {
                            c.mov(reg.r64(), x86::qword_ptr(input, (int32_t) sf.offset));
                            c.bswap(reg.r64());
                            c.movq(reg_d_p, reg.r64());
                        } else {
                            c.movsd(reg_d_p, x86::qword_ptr(input, (int32_t) sf.offset));
                        }
                        break;

                    default: Throw("StructConverter: unknown field type!");
                }

                if (sf.flags & Struct::EAssert) {
                    if (sf.is_integer()) {
                        auto ref = c.newInt64Const(asmjit::kConstScopeLocal, (int64_t) sf.default_);
                        c.cmp(reg.r64(), ref);
                    } else if (sf_type == Struct::EFloat32) {
                        auto ref = c.newFloatConst(asmjit::kConstScopeLocal, (float) sf.default_);
                        c.ucomiss(reg_f_p, ref);
                    } else if (sf_type == Struct::EFloat64) {
                        auto ref = c.newDoubleConst(asmjit::kConstScopeLocal, sf.default_);
                        c.ucomisd(reg_d_p, ref);
                    } else {
                        Throw("Internal error!");
                    }
                    failNeeded = true;
                    c.jne(loopFail);
                }

                if (!target->has_field(name))
                    continue;

                if (sf.is_integer() && (sf.flags & Struct::ENormalized)) {
                    auto range = sf.range();
                    float scale = float(1 / (range.second - range.first));
                    float offset = float(-range.first);

                    c.cvtsi2ss(reg_f_p, reg.r64());
                    if (offset != 0) {
                        auto offset_c = c.newFloatConst(asmjit::kConstScopeLocal, offset);
                        c.addss(reg_f_p, offset_c);
                    }
                    auto scale_c = c.newFloatConst(asmjit::kConstScopeLocal, scale);
                    c.mulss(reg_f_p, scale_c);

                    if (sf.flags & Struct::EGamma) {
                        auto call = c.call(imm_ptr((void *) math::inv_gamma<float>),
                            FuncBuilder1<float, float>(kCallConvHost));
                        call->setArg(0, reg_f_p);
                        call->setRet(0, reg_f_p);
                    }

                    sf_type = Struct::EFloat32;
                }

                if (sf_type_final == Struct::EInvalid) {
                    sf_type_final = sf_type;
                } else if (sf_type != sf_type_final) {
                    Throw("StructConverter(): field \"%s\" blends incompatible "
                          "fields.\nsource = %s,\ntarget = %s",
                          name, m_source->to_string(), m_target->to_string());
                }

                if (weight != 1.0) {
                    if (sf_type == Struct::EFloat32) {
                        auto scale = c.newFloatConst(asmjit::kConstScopeLocal, (float) weight);
                        c.mulss(reg_f_p, scale);
                    } else if (sf_type == Struct::EFloat64) {
                        auto scale = c.newDoubleConst(asmjit::kConstScopeLocal, (double) weight);
                        c.mulsd(reg_d_p, scale);
                    } else {
                        Throw("StructConverter(): field \"%s\": can't scale "
                              "non-float fields.\nsource = %s,\ntarget = %s",
                              name, m_source->to_string(), m_target->to_string());
                    }
                }

                if (reg_f != reg_f_p) {
                    if (sf_type == Struct::EFloat32) {
                        c.addss(reg_f, reg_f_p);
                    } else if (sf_type == Struct::EFloat64) {
                        c.addsd(reg_d, reg_d_p);
                    } else {
                        Throw("StructConverter(): field \"%s\": can't blend "
                              "non-float fields.\nsource = %s,\ntarget = %s",
                              name, m_source->to_string(), m_target->to_string());
                    }
                }
            }

            if (!target->has_field(name))
                continue;

            if (sf.is_integer() != df.is_integer()) {
                if (sf.is_integer()) {
                    if (df_type == Struct::EFloat16 || df_type == Struct::EFloat32)
                        c.cvtsi2ss(reg_f, reg.r64());
                    else if (df_type == Struct::EFloat64)
                        c.cvtsi2sd(reg_d, reg.r64());
                } else if (df.is_integer()) {
                    if (sf_type == Struct::EFloat32) {
                        auto range = df.range();
                        auto rbegin = c.newFloatConst(asmjit::kConstScopeLocal, float(range.first));
                        auto rend = c.newFloatConst(asmjit::kConstScopeLocal, float(range.second));

                        if (df.flags & Struct::ENormalized) {
                            if (df.flags & Struct::EGamma) {
                                auto call = c.call(imm_ptr((void *) math::gamma<float>),
                                    FuncBuilder1<float, float>(kCallConvHost));
                                call->setArg(0, reg_f);
                                call->setRet(0, reg_f);
                            }

                            auto rrange = c.newFloatConst(asmjit::kConstScopeLocal, float(range.second-range.first));
                            c.mulss(reg_f, rrange);
                            c.addss(reg_f, rbegin);
                        }

                        c.roundss(reg_f, reg_f, Imm(8));
                        c.maxss(reg_f, rbegin);
                        c.minss(reg_f, rend);
                        c.cvtss2si(reg.r64(), reg_f);
                    } else if (sf_type == Struct::EFloat64) {
                        auto range = df.range();
                        auto rbegin = c.newDoubleConst(asmjit::kConstScopeLocal, range.first);
                        auto rend = c.newDoubleConst(asmjit::kConstScopeLocal, range.second);

                        if (df.flags & Struct::ENormalized) {
                            if (df.flags & Struct::EGamma) {
                                auto call = c.call(imm_ptr((void *) math::gamma<double>),
                                    FuncBuilder1<double, double>(kCallConvHost));
                                call->setArg(0, reg_d);
                                call->setRet(0, reg_d);
                            }

                            auto rrange = c.newDoubleConst(asmjit::kConstScopeLocal, range.second-range.first);
                            c.mulsd(reg_d, rrange);
                            c.addsd(reg_d, rbegin);
                        }

                        c.roundsd(reg_d, reg_d, Imm(8));
                        c.maxsd(reg_d, rbegin);
                        c.minsd(reg_d, rend);
                        c.cvtsd2si(reg.r64(), reg_d);
                    }
                }
            }

            if (sf_type == Struct::EFloat32 && df_type == Struct::EFloat64)
                c.cvtss2sd(reg_d, reg_f);
            else if (sf_type == Struct::EFloat64 && (df_type == Struct::EFloat16 ||
                                                     df_type == Struct::EFloat32))
                c.cvtsd2ss(reg_f, reg_d);
        } catch (const std::exception &e) {
            if (!(df.flags & Struct::EDefault))
                Throw("StructConverter(): field \"%s\" is missing in the "
                      "source structure: %s\nsource = %s,\ntarget = %s",
                      name, e.what(), m_source->to_string(), m_target->to_string());

            if (df.is_integer()) {
                if (df.default_ == 0) {
                    c.xor_(reg, reg);
                } else {
                    auto cval = c.newInt64Const(asmjit::kConstScopeLocal, df.default_);
                    c.mov(reg, cval);
                }
            } else if (df_type == Struct::EFloat16 || df_type == Struct::EFloat32) {
                if (df.default_ == 0) {
                    c.xorps(reg_f, reg_f);
                } else {
                    auto cval = c.newFloatConst(asmjit::kConstScopeLocal, (float) df.default_);
                    c.movd(reg_f, cval);
                }
            } else if (df_type == Struct::EFloat64) {
                if (df.default_ == 0) {
                    c.xorpd(reg_d, reg_d);
                } else {
                    auto cval = c.newDoubleConst(asmjit::kConstScopeLocal, df.default_);
                    c.movq(reg_d, cval);
                }
            }
        }

        bool target_swap = target->byte_order() == Struct::EBigEndian;
        switch (df_type) {
            case Struct::EUInt8:
            case Struct::EInt8:
                c.mov(x86::byte_ptr(output, (int32_t) df.offset), reg.r8());
                break;

            case Struct::EFloat16: {
                    #if defined(__F16C__)
                        c.vcvtps2ph(reg_f, reg_f, 0);
                        c.movd(reg.r32(), reg_f);
                    #else
                        auto call = c.call(imm_ptr((void *) enoki::half::float32_to_float16),
                            FuncBuilder1<uint16_t, float>(kCallConvHost));
                        call->setArg(0, reg_f);
                        call->setRet(0, reg);
                    #endif


                    if (target_swap)
                       c.xchg(reg.r8Lo(), reg.r8Hi());

                    c.mov(x86::word_ptr(output, (int32_t) df.offset), reg.r16());
                }
                break;

            case Struct::EUInt16:
            case Struct::EInt16:
                if (target_swap)
                   c.xchg(reg.r8Lo(), reg.r8Hi());

                c.mov(x86::word_ptr(output, (int32_t) df.offset), reg.r16());
                break;

            case Struct::EUInt32:
            case Struct::EInt32:
                if (target_swap)
                    c.bswap(reg.r32());

                c.mov(x86::dword_ptr(output, (int32_t) df.offset), reg.r32());
                break;

            case Struct::EUInt64:
            case Struct::EInt64:
                if (target_swap)
                    c.bswap(reg.r64());

                c.mov(x86::qword_ptr(output, (int32_t) df.offset), reg.r64());
                break;

            case Struct::EFloat32:
                if (target_swap) {
                    c.movd(reg.r32(), reg_f);
                    c.bswap(reg.r32());
                    c.mov(x86::dword_ptr(output, (int32_t) df.offset), reg.r32());
                } else {
                    c.movss(x86::dword_ptr(output, (int32_t) df.offset), reg_f);
                }
                break;

            case Struct::EFloat64:
                if (target_swap) {
                    c.movq(reg.r64(), reg_d);
                    c.bswap(reg.r64());
                    c.mov(x86::qword_ptr(output, (int32_t) df.offset), reg.r64());
                } else {
                    c.movsd(x86::qword_ptr(output, (int32_t) df.offset), reg_d);
                }
                break;

            default: Throw("StructConverter: unknown field type!");
        }
    }

    c.add(input,  Imm(source->size()));
    c.add(output, Imm(target->size()));
    c.cmp(input, count);
    c.jne(loopStart);
    c.bind(loopEnd);

    auto rv = c.newInt64("rv");
    c.mov(rv.r32(), 1);
    c.ret(rv);
    if (failNeeded) {
        c.bind(loopFail);
        c.xor_(rv, rv);
        c.ret(rv);
    }
    c.endFunc();
    c.finalize();

    #if MTS_JIT_LOG_ASSEMBLY == 1
       Log(EInfo, "Assembly:\n%s", logger.getString());
    #endif

    m_func = asmjit_cast<FuncType>(assembler.make());
    __cache[key] = (void *) m_func;
#endif
}

#if MTS_STRUCTCONVERTER_USE_JIT == 0
bool StructConverter::convert(size_t count, const void *src_, void *dest_) const {
    size_t source_size = m_source->size();
    size_t target_size = m_target->size();
    using namespace mitsuba::detail;

    /* Is swapping needed */
    bool source_swap = m_source->byte_order() != Struct::host_byte_order();
    bool target_swap = m_target->byte_order() != Struct::host_byte_order();

    std::vector<std::string> fields;
    for (Struct::Field f : *m_source) {
        if ((f.flags & Struct::EAssert) && !m_target->has_field(f.name))
            fields.push_back(f.name);
    }
    for (Struct::Field f : *m_target)
        fields.push_back(f.name);

    for (size_t i = 0; i<count; ++i) {
        for (auto const &name : fields) {

            Struct::Field df;
            if (m_target->has_field(name))
                df = m_target->field(name);

            int df_type = df.type;
            if (df_type == Struct::EFloat) {
                if (sizeof(Float) == sizeof(float))
                    df_type = Struct::EFloat32;
                else if (sizeof(Float) == sizeof(double))
                    df_type = Struct::EFloat64;
                else
                    Throw("Invalid 'Float' size");
            }

            int64_t reg = 0;
            float reg_f = 0, accum_f = 0;
            double reg_d = 0, accum_d = 0;
            Struct::EType sf_type_final = Struct::EInvalid;
            Struct::Field sf;
            Struct::EType &sf_type = sf.type;

            try {
                auto sources = df.blend;
                if (sources.empty())
                    sources.push_back(std::make_pair(1.0, name));

                for (size_t j = 0; j < sources.size(); ++j) {
                    double weight = sources[j].first;
                    sf = m_source->field(sources[j].second);

                    const uint8_t *src =
                        (const uint8_t *) src_ + sf.offset + source_size * i;

                    if (sf_type == Struct::EFloat) {
                        if (sizeof(Float) == sizeof(float))
                            sf_type = Struct::EFloat32;
                        else if (sizeof(Float) == sizeof(double))
                            sf_type = Struct::EFloat64;
                        else
                            Throw("Invalid 'Float' size");
                    }

                    switch (sf_type) {
                        case Struct::EUInt8:
                            reg = ((const uint8_t *)src)[0];
                            break;

                        case Struct::EInt8:
                            reg = (int64_t) *((const int8_t *)  src);
                            break;

                        case Struct::EUInt16: {
                                uint16_t val = *((const uint16_t *) src);
                                if (source_swap)
                                    val = swap16(val);
                                reg = (int64_t) val;
                            }
                            break;

                        case Struct::EInt16: {
                                int16_t val = *((const int16_t *) src);
                                if (source_swap)
                                    val = swap16(val);
                                reg = (int64_t) val;
                            }
                            break;

                        case Struct::EUInt32: {
                                uint32_t val = *((const uint32_t *) src);
                                if (source_swap)
                                    val = swap32(val);
                                reg = (int64_t) val;
                            }
                            break;

                        case Struct::EInt32: {
                                int32_t val = *((const int32_t *) src);
                                if (source_swap)
                                    val = swap32(val);
                                reg = (int64_t) val;
                            }
                            break;

                        case Struct::EUInt64: {
                                uint64_t val = *((const uint64_t *) src);
                                if (source_swap)
                                    val = swap64(val);
                                reg = (int64_t) val;
                            }
                            break;

                        case Struct::EInt64: {
                                int64_t val = *((const int64_t *) src);
                                if (source_swap)
                                    val = swap64(val);
                                reg = val;
                            }
                            break;

                        case Struct::EFloat16: {
                                uint16_t val = *((const uint16_t *) src);
                                if (source_swap)
                                    val = swap16(val);
                                reg_f = enoki::half::float16_to_float32(val);
                                sf_type = Struct::EFloat32;
                            }
                            break;

                        case Struct::EFloat32: {
                                uint32_t val = *((const uint32_t *) src);
                                if (source_swap)
                                    val = swap32(val);
                                reg_f = memcpy_cast<float>(val);
                            }
                            break;

                        case Struct::EFloat64: {
                                uint64_t val = *((const uint64_t *) src);
                                if (source_swap)
                                    val = swap64(val);
                                reg_d = memcpy_cast<double>(val);
                            }
                            break;

                        default: Throw("StructConverter: unknown field type!");
                    }

                    if (sf.flags & Struct::EAssert) {
                        if (sf.is_integer() && (int64_t) sf.default_ != reg)
                            return false;
                        else if (sf_type == Struct::EFloat32 && (float) sf.default_ != reg_f)
                            return false;
                        else if (sf_type == Struct::EFloat64 && sf.default_ != reg_d)
                            return false;
                    }

                    if (!m_target->has_field(name))
                        continue;

                    if (sf.is_integer() && (sf.flags & Struct::ENormalized)) {
                        auto range = sf.range();
                        float scale = float(1 / (range.second - range.first));
                        float offset = float(-range.first);
                        reg_f = ((float) reg + offset) * scale;
                        if (sf.flags & Struct::EGamma)
                            reg_f = math::inv_gamma<float>(reg_f);
                        sf_type = Struct::EFloat32;
                    }

                    accum_f += reg_f * (float) weight;
                    accum_d += reg_d * weight;

                    if ((weight != 1.0 || sources.size() > 1) && !sf.is_float()) {
                        Throw("StructConverter(): field \"%s\": can't scale "
                              "non-float fields.\nsource = %s,\ntarget = %s",
                              name, m_source->to_string(), m_target->to_string());
                    }

                    if (sf_type_final == Struct::EInvalid) {
                        sf_type_final = sf_type;
                    } else if (sf_type != sf_type_final) {
                        Throw("StructConverter(): field \"%s\" blends incompatible "
                              "fields.\nsource = %s,\ntarget = %s",
                              name, m_source->to_string(), m_target->to_string());
                    }
                }

                if (!m_target->has_field(name))
                    continue;

                reg_f = accum_f;
                reg_d = accum_d;

                if (sf_type == Struct::EFloat32 && df_type == Struct::EFloat64)
                    reg_d = (double) reg_f;
                else if (sf_type == Struct::EFloat64 && (df_type == Struct::EFloat16 ||
                                                         df_type == Struct::EFloat32))
                    reg_f = (float) reg_d;
            } catch (const std::exception &e) {
                if (!(df.flags & Struct::EDefault))
                    Throw("StructConverter(): field \"%s\" is missing in the "
                          "source structure: %s\nsource = %s,\ntarget = %s",
                          name, e.what(), m_source->to_string(), m_target->to_string());

                if (df.is_integer())
                    reg = (int64_t) df.default_;
                else if (df_type == Struct::EFloat16 || df_type == Struct::EFloat32)
                    reg_f = (float) df.default_;
                else if (df_type == Struct::EFloat64)
                    reg_d = df.default_;
            }

            if (sf.is_integer() != df.is_integer()) {
                if (sf.is_integer()) {
                    if (df_type == Struct::EFloat16 || df_type == Struct::EFloat32)
                        reg_f = (float) reg;
                    else if (df_type == Struct::EFloat64)
                        reg_d = (double) reg;
                } else if (df.is_integer()) {
                    if (sf_type == Struct::EFloat32) {
                        auto range = df.range();

                        if (df.flags & Struct::ENormalized) {
                            if (df.flags & Struct::EGamma)
                                reg_f = math::gamma<float>(reg_f);
                            reg_f = reg_f * (float) (range.second - range.first) + (float) range.first;
                        }

                        reg_f = std::rint(reg_f);
                        reg_f = std::max(reg_f, (float) range.first);
                        reg_f = std::min(reg_f, (float) range.second);
                        reg = (int64_t) reg_f;
                    } else if (sf_type == Struct::EFloat64) {
                        auto range = df.range();

                        if (df.flags & Struct::ENormalized) {
                            if (df.flags & Struct::EGamma)
                                reg_d = math::gamma<double>(reg_d);
                            reg_d = reg_d * (range.second - range.first) + range.first;
                        }

                        reg_d = std::rint(reg_d);
                        reg_d = std::max(reg_d, range.first);
                        reg_d = std::min(reg_d, range.second);
                        reg = (int64_t) reg_d;
                    }
                }
            }

            uint8_t *dst = (uint8_t *) dest_ + df.offset + target_size * i;
            switch (df_type) {
                case Struct::EUInt8:
                    *((uint8_t *) dst) = (uint8_t) reg;
                    break;

                case Struct::EInt8:
                    *((int8_t *) dst) = (int8_t) reg;
                    break;

                case Struct::EUInt16: {
                        uint16_t val = (uint16_t) reg;
                        if (target_swap)
                            val = swap16(val);
                        *((uint16_t *) dst) = val;
                    }
                    break;

                case Struct::EInt16: {
                        int16_t val = (int16_t) reg;
                        if (target_swap)
                            val = swap16(val);
                        *((int16_t *) dst) = val;
                    }
                    break;

                case Struct::EUInt32: {
                        uint32_t val = (uint32_t) reg;
                        if (target_swap)
                            val = swap32(val);
                        *((uint32_t *) dst) = val;
                    }
                    break;

                case Struct::EInt32: {
                        int32_t val = (int32_t) reg;
                        if (target_swap)
                            val = swap32(val);
                        *((int32_t *) dst) = val;
                    }
                    break;

                case Struct::EUInt64: {
                        uint64_t val = (uint64_t) reg;
                        if (target_swap)
                            val = swap64(val);
                        *((uint64_t *) dst) = val;
                    }
                    break;

                case Struct::EInt64: {
                        int64_t val = (int64_t) reg;
                        if (target_swap)
                            val = swap64(val);
                        *((int64_t *) dst) = val;
                    }
                    break;

                case Struct::EFloat16: {
                        uint16_t val = enoki::half::float32_to_float16(reg_f);
                        if (target_swap)
                            val = swap16(val);
                        *((uint16_t *) dst) = val;
                    }
                    break;

                case Struct::EFloat32: {
                        uint32_t val = (uint32_t) memcpy_cast<uint32_t>(reg_f);
                        if (target_swap)
                            val = swap32(val);
                        *((uint32_t *) dst) = val;
                    }
                    break;

                case Struct::EFloat64: {
                        uint64_t val = (uint64_t) memcpy_cast<uint64_t>(reg_d);
                        if (target_swap)
                            val = swap64(val);
                        *((uint64_t *) dst) = val;
                    }
                    break;

                default: Throw("StructConverter: unknown field type!");
            }
        }
    }
    return true;
}
#endif

std::string StructConverter::to_string() const {
    std::ostringstream oss;
    oss << "StructConverter[" << std::endl
        << "  source = " << string::indent(m_source->to_string()) << "," << std::endl
        << "  target = " << string::indent(m_target->to_string()) << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(Struct, Object)
MTS_IMPLEMENT_CLASS(StructConverter, Object)
NAMESPACE_END(mitsuba)
