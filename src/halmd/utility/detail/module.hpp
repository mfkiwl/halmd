/*
 * Copyright © 2008-2010  Peter Colberg
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HALMD_UTILITY_DETAIL_MODULE_HPP
#define HALMD_UTILITY_DETAIL_MODULE_HPP

#include <boost/shared_ptr.hpp>
#include <exception>
#include <set>
#include <typeinfo>

#include <halmd/utility/detail/builder.hpp>
#include <halmd/utility/detail/exception.hpp>
#include <halmd/utility/detail/factory.hpp>
#include <halmd/utility/options.hpp>
#include <halmd/util/logger.hpp>

namespace halmd
{
namespace utility { namespace detail
{

// import into namespace
using boost::dynamic_pointer_cast;
using boost::shared_ptr;

/**
 * Concrete module
 */
template <typename T>
class module
  : public builder<T>
{
public:
    typedef typename T::module_type _Base;
    typedef typename factory<_Base>::builder_set builder_set;
    typedef typename builder_set::iterator builder_iterator;

    /**
     * returns singleton instance
     */
    static shared_ptr<T> fetch(po::options const& vm)
    {
        LOG_DEBUG("fetch module " << typeid(T).name());
        return dynamic_pointer_cast<T>(factory<_Base>::fetch(vm));
    }

    static void resolve(po::options const& vm);

    /**
     * returns module name
     */
    static std::string name()
    {
        return typeid(T).name();
    }

    module() : resolved_(false) {}

protected:
    /**
     * weak module ordering
     */
    bool _rank(shared_ptr<builder<_Base> > const& other) const
    {
        // For the case that the *other* module derives from
        // *this* module and should thus be ranked higher than
        // this module, returns false. Otherwise returns true.
        return !dynamic_pointer_cast<builder<T> >(other);
    }

    /**
     * creates and returns module instance
     */
    shared_ptr<_Base> _create(po::options const& vm)
    {
        LOG_DEBUG("create module " << typeid(T).name());
        return shared_ptr<_Base>(new T(vm));
    }

    /**
     * returns module options
     */
    void _options(po::options_description& desc)
    {
        T::options(desc);
    }

    /**
     * resolve module dependencies
     */
    void _resolve(po::options const& vm)
    {
        if (!resolved_) {
            LOG_DEBUG("resolve module " + std::string(typeid(T).name()));
            T::resolve(vm);
            // cache result
            resolved_ = true;
        }
    }

private:
    struct _register
    {
        _register()
        {
            factory<_Base>::_register(shared_ptr<builder<_Base> >(new module<T>));
        }
    };

    static _register register_;
    bool resolved_;
};

template <typename T> typename module<T>::_register module<T>::register_;

/**
 * resolve dependencies for given module
 */
template <typename T>
void module<T>::resolve(po::options const& vm)
{
    LOG_DEBUG("resolve builder " << typeid(T).name());
    builder_set& builders = factory<_Base>::builders();

    for (builder_iterator it = builders.begin(); it != builders.end(); ) {
        if (!dynamic_pointer_cast<builder<T> >(*it)) {
            // module does not implement builder specification
            builders.erase(it++);
        }
        else {
            try {
                (*it)->_resolve(vm);
                // resolvable builder
                return;
            }
            catch (module_exception const& e) {
                // irresolvable builder
                LOG_DEBUG(e.what());
                builders.erase(it++);
            }
        }
    }
    // no suitable modules available
    throw module_exception("irresolvable module " + module<T>::name());
}

/**
 * Type-independent module interface
 */
template <>
class module<>
{
public:
    /**
     * returns options of resolved modules
     */
    static po::options_description options()
    {
        po::options_description desc;
        factory<>::options(desc);
        return desc;
    }
};

}} // namespace utility::detail

} // namespace halmd

#endif /* ! HALMD_UTILITY_DETAIL_MODULE_HPP */
