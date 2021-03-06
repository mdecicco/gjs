#include <gjs/common/script_function.h>
#include <gjs/common/script_type.h>
#include <gjs/common/script_context.h>
#include <gjs/common/script_module.h>
#include <gjs/vm/register.h>
#include <gjs/bind/bind.h>
#include <gjs/util/util.h>

namespace gjs {
    script_function::script_function(script_context* ctx, const std::string _name, address addr, script_module* mod) {
        m_ctx = ctx;

        name = _name;
        is_host = false;
        is_static = false;
        is_method_of = nullptr;

        signature.return_type = nullptr;
        signature.return_loc = vm_register::register_count;
        signature.returns_on_stack = false;
        signature.is_thiscall = false;
        signature.returns_pointer = false;
        signature.is_subtype_obj_ctor = false;

        access.entry = addr;

        owner = mod;
    }

    script_function::script_function(type_manager* mgr, script_type* tp, bind::wrapped_function* wrapped, bool is_ctor, bool is_dtor, script_module* mod) {
        m_ctx = mgr->m_ctx;

        name = wrapped->name;
        is_host = true;
        is_static = wrapped->is_static_method;
        is_method_of = tp;

        // return type
        if (std::string(wrapped->return_type.name()) == "void" && wrapped->ret_is_ptr) {
            signature.return_type = m_ctx->types()->get<void*>();
        } else signature.return_type = m_ctx->types()->get(wrapped->return_type.name());

        if (!signature.return_type) {
            throw bind_exception(format("Return value of function '%s' is of type '%s' that has not been bound yet", name.c_str(), wrapped->return_type.name()));
        }

        signature.return_loc = vm_register::v0;
        signature.returns_on_stack = !wrapped->ret_is_ptr && !signature.return_type->is_primitive && signature.return_type->size != 0;
        signature.is_thiscall = tp && !wrapped->is_static_method;
        signature.returns_pointer = wrapped->ret_is_ptr;
        signature.is_subtype_obj_ctor = tp && tp->requires_subtype && is_ctor;

        u16 gp_arg = 0;
        u16 fp_arg = 0;

        if (signature.is_thiscall) gp_arg++;
        if (signature.returns_on_stack) gp_arg++;

        // args
        for (u8 i = 0;i < wrapped->arg_types.size();i++) {
            script_type* atp = nullptr;
            if (std::string(wrapped->arg_types[i].name()) == "void" && wrapped->arg_is_ptr[i]) {
                atp = m_ctx->types()->get<void*>(); // some object or primitive pointer
            } else atp = m_ctx->types()->get(wrapped->arg_types[i].name());

            if (!atp) {
                throw bind_exception(format("Arg '%d' of function '%s' is of type '%s' that has not been bound yet", i + 1, name.c_str(), wrapped->arg_types[i].name()));
            }
            signature.arg_types.push_back(atp);

            if (atp->is_floating_point) signature.arg_locs.push_back(vm_register(gp_arg++));
            else signature.arg_locs.push_back(vm_register(fp_arg++));
        }

        access.wrapped = wrapped;

        owner = mod ? mod : mgr->m_ctx->global();
        owner->add(this);
    }

    void script_function::arg(script_type* type) {
        if (is_host) throw bind_exception("Cannot specify arguments for host functions");
        if (!type) throw bind_exception("No type specified for argument");
        signature.arg_types.push_back(type);
    }
};