#include <gjs/common/script_function.h>
#include <gjs/common/script_type.h>
#include <gjs/common/script_context.h>
#include <gjs/util/util.h>
#include <gjs/bind/bind.h>

namespace gjs {
    type_manager::type_manager(script_context* ctx) : m_ctx(ctx) {
    }

    type_manager::~type_manager() {
    }

    void type_manager::merge(type_manager* new_types) {
        auto all = new_types->all();
        new_types->m_types.clear();
        new_types->m_types_by_id.clear();

        for (u16 i = 0;i < all.size();i++) {
            if (m_types.count(all[i]->name) > 0) {
                throw bind_exception(format("Type '%s' already bound", all[i]->name.c_str()));
            }
            if (m_types_by_id.count(all[i]->id()) > 0) {
                throw bind_exception(format("There was a type id collision while binding type '%s'", all[i]->name.c_str()));
            }

            m_types[all[i]->name] = all[i];
            m_types_by_id[all[i]->id()] = all[i];
        }
    }

    script_type* type_manager::get(const std::string& name) {
        if (m_types.count(name) == 0) return nullptr;
        return m_types[name];
    }

    script_type* type_manager::get(u32 id) {
        if (m_types_by_id.count(id) == 0) return nullptr;
        return m_types_by_id[id];
    }

    script_type* type_manager::add(const std::string& name, const std::string& internal_name) {
        if (m_types.count(internal_name) > 0) {
            throw bind_exception(format("Type '%s' already bound", name.c_str()));
        }
        u32 id = hash(name);
        if (m_types_by_id.count(id) > 0) {
            throw bind_exception(format("There was a type id collision while binding type '%s'", name.c_str()));
        }

        script_type* t = new script_type();
        t->m_id = id;
        t->name = name;
        t->internal_name = internal_name;

        m_types[internal_name] = t;
        m_types_by_id[id] = t;

        m_ctx->types()->add(t);

        return t;
    }
    
    void type_manager::add(script_type* type) {
        if (m_types.count(type->internal_name) > 0) {
            throw bind_exception(format("Type '%s' already bound", type->name.c_str()));
        }

        if (m_types_by_id.count(type->id()) > 0) {
            throw bind_exception(format("There was a type id collision while binding type '%s'", type->name.c_str()));
        }

        m_types[type->internal_name] = type;
        m_types_by_id[type->m_id] = type;
    }

    script_type* type_manager::finalize_class(bind::wrapped_class* wrapped, script_module* mod) {
        auto it = m_types.find(wrapped->internal_name);
        if (it == m_types.end()) {
            throw bind_exception(format("Type '%s' not found and can not be finalized", wrapped->name.c_str()));
        }

        if (wrapped->dtor) {
            // gjs considers constructors and destructors methods, but they are
            // bound like regular C functions. Remove explicit 'this' argument
            // because gjs implicitly adds it.
            wrapped->dtor->arg_types.erase(wrapped->dtor->arg_types.begin());
            wrapped->dtor->arg_is_ptr.erase(wrapped->dtor->arg_is_ptr.begin());
        }

        script_type* t = it->getSecond();
        t->is_pod = wrapped->is_pod;
        t->m_wrapped = wrapped;
        t->is_trivially_copyable = wrapped->trivially_copyable;
        t->pass_ret = wrapped->pass_ret;
        t->requires_subtype = false;
        t->size = (u32)wrapped->size;
        t->destructor = wrapped->dtor ? new script_function(this, t, wrapped->dtor, false, true, mod) : nullptr;

        for (auto i = wrapped->properties.begin();i != wrapped->properties.end();++i) {
            t->properties.push_back({
                i->getSecond()->flags,
                i->getFirst(),
                get(i->getSecond()->type.name()),
                i->getSecond()->offset,
                i->getSecond()->getter ? new script_function(this, t, i->getSecond()->getter, false, false, mod) : nullptr,
                i->getSecond()->setter ? new script_function(this, t, i->getSecond()->setter, false, false, mod) : nullptr
            });

            if (i->getSecond()->type == std::type_index(typeid(subtype_t))) {
                t->requires_subtype = true;
            }
        }

        for (u32 i = 0;i < wrapped->methods.size();i++) {
            bind::wrapped_function* f = wrapped->methods[i];
            for (u8 a = 0;a < f->arg_types.size();a++) {
                if (f->arg_types[a] == std::type_index(typeid(subtype_t))) {
                    t->requires_subtype = true;
                }
            }
        }

        for (u32 i = 0;i < wrapped->methods.size();i++) {
            bind::wrapped_function* f = wrapped->methods[i];
            if (f->name.find("::constructor") != std::string::npos) {
                // gjs considers constructors and destructors methods, but they are
                // bound like regular C functions. Remove explicit 'this' argument
                // because gjs implicitly adds it.
                f->arg_types.erase(f->arg_types.begin());
                f->arg_is_ptr.erase(f->arg_is_ptr.begin());

                if (t->requires_subtype) {
                    // second argument is a moduletype id, but should not be
                    // explicitly listed as an argument. gjs deals with passing
                    // that parameter internally
                    f->arg_is_ptr.erase(f->arg_is_ptr.begin());
                    f->arg_types.erase(f->arg_types.begin());
                }

                t->methods.push_back(new script_function(this, t, f, true, false, mod));
                continue;
            }
            t->methods.push_back(new script_function(this, t, f, false, false, mod));
        }

        t->owner = mod;
        return t;
    }

    std::vector<script_type*> type_manager::all() {
        std::vector<script_type*> out;
        for (auto i = m_types.begin();i != m_types.end();++i) {
            out.push_back(i->getSecond());
        }
        return out;
    }

    script_context* type_manager::ctx() {
        return m_ctx;
    }


    script_type::script_type() {
        m_id = 0;
        size = 0;
        is_pod = false;
        is_primitive = false;
        is_floating_point = false;
        is_unsigned = false;
        is_builtin = false;
        is_host = false;
        requires_subtype = false;
        owner = nullptr;
        destructor = nullptr;
        m_wrapped = nullptr;
        base_type = nullptr;
        sub_type = nullptr;
    }

    script_type::~script_type() {
        if (m_wrapped) delete m_wrapped;
    }

    script_function* script_type::method(const std::string& _name, script_type* ret, const std::vector<script_type*>& arg_types) {
        return function_search(name + "::" + _name, methods, ret, arg_types);
    }

    script_type::property* script_type::prop(const std::string& _name) {
        for (u16 i = 0;i < properties.size();i++) {
            if (properties[i].name == _name) return &properties[i];
        }

        return nullptr;
    }
};