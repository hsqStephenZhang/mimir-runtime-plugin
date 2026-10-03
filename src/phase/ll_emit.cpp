#include <iomanip>
#include <sstream>

#include <mim/lam.h>
#include <mim/tuple.h>

#include <mim/plug/ll/ll.h>

#include "mim/plug/runtime/runtime.h"

using namespace std::string_literals;

namespace mim::plug::runtime {

namespace {

std::string llvm_string(std::string_view value) {
    std::ostringstream escaped;
    for (unsigned char c : value) {
        if (c >= 0x20 && c < 0x7f && c != '\\' && c != '"') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '\\' << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned>(c) << std::dec << std::setfill(' ');
        }
    }
    escaped << "\\00";
    return escaped.str();
}

/// `ll.emit` plus host lowering of the backend-neutral runtime checks.
class Emitter : public ll::Emitter {
public:
    using Super = ll::Emitter;
    using Super::Super;

    void emit_epilogue(Lam*) override;
    std::optional<std::string> isa_targetspecific_intrinsic(ll::BB&, const Def*) override;

private:
    std::string message_global(const Def* def, std::string_view prefix, std::string_view message);
    void emit_fail(ll::BB&, const Def*);
    void emit_assert(ll::BB&, const Def*);
};

std::string Emitter::message_global(const Def* def, std::string_view prefix, std::string_view message) {
    auto global = std::format("@.mimir_{}_{}", prefix, def->gid());
    std::print(vars_decls_, "{} = private unnamed_addr constant [{} x i8] c\"{}\"\n", global, message.size() + 1,
               llvm_string(message));
    declare("i32 @puts(ptr)");
    declare("i32 @fflush(ptr)");
    declare("void @abort() noreturn");
    return global;
}

void Emitter::emit_fail(ll::BB& bb, const Def* def) {
    auto fail    = Axm::expect<runtime::fail>(def, "a runtime failure");
    auto message = tuple2str(fail->arg());
    auto global  = message_global(def, "fail", message);

    bb.tail("call i32 @puts(ptr {})", global);
    bb.tail("call i32 @fflush(ptr null)");
    bb.tail("call void @abort()");
}

void Emitter::emit_assert(ll::BB& bb, const Def* def) {
    auto check     = Axm::expect<runtime::assert>(def, "a runtime assertion");
    auto condition = check->arg()->proj(3, 1);
    auto message   = tuple2str(check->arg()->proj(3, 2));
    auto global    = message_global(def, "assert_msg", message);
    auto helper    = std::format("@.mimir_assert_{}", def->gid());

    // A branch cannot be spliced into the middle of a basic block, so the check lives in its own function.
    std::print(vars_decls_,
               "define internal void {}(i1 %condition) {{\n"
               "entry:\n"
               "    br i1 %condition, label %success, label %failure\n"
               "failure:\n"
               "    call i32 @puts(ptr {})\n"
               "    call i32 @fflush(ptr null)\n"
               "    call void @abort()\n"
               "    unreachable\n"
               "success:\n"
               "    ret void\n"
               "}}\n\n",
               helper, global);

    emit_unsafe(check->arg()->proj(3, 0));
    bb.tail("call void {}(i1 {})", helper, emit(condition));
}

void Emitter::emit_epilogue(Lam* lam) {
    // A `runtime.fail` argument never reaches its callee.
    // Emit it in this predecessor instead of letting the scheduler hoist it into another block.
    if (auto app = lam->body()->isa<App>())
        for (auto arg : app->args())
            if (Axm::isa<runtime::fail>(arg)) {
                auto& bb = lam2bb_[lam];
                emit_fail(bb, arg);
                return bb.tail("unreachable");
            }

    Super::emit_epilogue(lam);
}

std::optional<std::string> Emitter::isa_targetspecific_intrinsic(ll::BB& bb, const Def* def) {
    if (auto check = Axm::isa<runtime::static_check>(def)) return emit(check->arg()->proj(2, 0));
    if (Axm::isa<runtime::fail>(def)) return emit_fail(bb, def), "undef"s;
    if (Axm::isa<runtime::assert>(def)) return emit_assert(bb, def), ""s;
    return {};
}

class Emit : public Phase {
public:
    Emit(World& world, flags_t annex)
        : Phase(world, annex) {}

    void start() override {
        auto name = world().name() ? world().name().str() : "a"s;
        auto path = name + ".ll"s;
        if (auto o = arg_value(args(), "o", "output")) path = *o;
        auto rt = arg_value(args(), "rt") == "extern" ? Emitter::Rt::ext : Emitter::Rt::embed;

        auto out     = Out(path);
        auto emitter = Emitter(world(), "llvm_runtime_emitter", *out.os());
        emitter.rt_mode(rt);
        if (rt == Emitter::Rt::embed) emitter.load_rt_module("ll_rt.ll");
        emitter.run();
    }
};

} // namespace

void register_phases(Flags2Phases& phases) { Phase::hook<runtime::ll_emit, Emit>(phases); }

} // namespace mim::plug::runtime
