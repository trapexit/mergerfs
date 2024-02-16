#ifndef BOOST_LEAF_ERROR_HPP_INCLUDED
#define BOOST_LEAF_ERROR_HPP_INCLUDED

// Copyright 2018-2023 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/detail/optional.hpp>
#include <boost/leaf/detail/function_traits.hpp>
#include <boost/leaf/detail/print.hpp>

#if BOOST_LEAF_CFG_DIAGNOSTICS
#   include <ostream>
#endif

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR
#   include <system_error>
#endif

#if BOOST_LEAF_CFG_CAPTURE
#   include <memory>
#endif

#define BOOST_LEAF_TOKEN_PASTE(x, y) x ## y
#define BOOST_LEAF_TOKEN_PASTE2(x, y) BOOST_LEAF_TOKEN_PASTE(x, y)
#define BOOST_LEAF_TMP BOOST_LEAF_TOKEN_PASTE2(boost_leaf_tmp_, __LINE__)

#define BOOST_LEAF_ASSIGN(v,r)\
    auto && BOOST_LEAF_TMP = r;\
    static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
        "BOOST_LEAF_ASSIGN/BOOST_LEAF_AUTO requires a result object as the second argument (see is_result_type)");\
    if( !BOOST_LEAF_TMP )\
        return BOOST_LEAF_TMP.error();\
    v = std::forward<decltype(BOOST_LEAF_TMP)>(BOOST_LEAF_TMP).value()

#define BOOST_LEAF_AUTO(v, r)\
    BOOST_LEAF_ASSIGN(auto v, r)

#if BOOST_LEAF_CFG_GNUC_STMTEXPR

#define BOOST_LEAF_CHECK(r)\
        ({\
            auto && BOOST_LEAF_TMP = (r);\
            static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
                "BOOST_LEAF_CHECK requires a result object (see is_result_type)");\
            if( !BOOST_LEAF_TMP )\
                return BOOST_LEAF_TMP.error();\
            std::move(BOOST_LEAF_TMP);\
        }).value()

#else

#define BOOST_LEAF_CHECK(r)\
        {\
            auto && BOOST_LEAF_TMP = (r);\
            static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
                "BOOST_LEAF_CHECK requires a result object (see is_result_type)");\
            if( !BOOST_LEAF_TMP )\
                return BOOST_LEAF_TMP.error();\
        }

#endif

#define BOOST_LEAF_NEW_ERROR ::boost::leaf::leaf_detail::inject_loc{__FILE__,__LINE__,__FUNCTION__}+::boost::leaf::new_error

namespace boost { namespace leaf {

class error_id;

namespace leaf_detail
{
    struct BOOST_LEAF_SYMBOL_VISIBLE tls_tag_unexpected_enabled_counter;
    struct BOOST_LEAF_SYMBOL_VISIBLE tls_tag_id_factory_current_id;

    struct inject_loc
    {
        char const * const file;
        int const line;
        char const * const fn;

        template <class T>
        friend T operator+( inject_loc loc, T && x ) noexcept
        {
            x.load_source_location_(loc.file, loc.line, loc.fn);
            return std::move(x);
        }
    };
}

} }

////////////////////////////////////////

namespace boost { namespace leaf {

#if BOOST_LEAF_CFG_DIAGNOSTICS

namespace leaf_detail
{
    class BOOST_LEAF_SYMBOL_VISIBLE e_unexpected_count
    {
    public:

        char const * (*first_type)();
        int count;

        BOOST_LEAF_CONSTEXPR explicit e_unexpected_count(char const * (*ft)()) noexcept:
            first_type(ft),
            count(1)
        {
        }

        template <class CharT, class Traits>
        void print( std::basic_ostream<CharT, Traits> & os ) const
        {
            BOOST_LEAF_ASSERT(first_type != nullptr);
            BOOST_LEAF_ASSERT(count>0);
            os << "Detected ";
            if( count==1 )
                os << "1 attempt to communicate an unexpected error object";
            else
                os << count << " attempts to communicate unexpected error objects, the first one";
            (os << " of type " << first_type() << '\n').flush();
        }
    };

    template <>
    struct diagnostic<e_unexpected_count, false, false>
    {
        static constexpr bool is_invisible = true;

        template <class CharT, class Traits>
        BOOST_LEAF_CONSTEXPR static void print( std::basic_ostream<CharT, Traits> &, e_unexpected_count const &) noexcept { }
    };

    template <class E>
    class BOOST_LEAF_SYMBOL_VISIBLE slot;

    class BOOST_LEAF_SYMBOL_VISIBLE e_unexpected_info
    {
        e_unexpected_info( e_unexpected_info const & ) = delete;
        e_unexpected_info & operator=( e_unexpected_info const & ) = delete;

        struct slot_base
        {
        public:
            slot_base * next_;
            virtual void deactivate() noexcept = 0;
            virtual void propagate( int err_id ) noexcept = 0;
            virtual void print( std::ostream &, int key_to_print ) const = 0;
            virtual ~slot_base() noexcept { };
        protected:
            BOOST_LEAF_CONSTEXPR slot_base():
                next_(nullptr)
            {
            }
        };

        template <class E>
        class slot_store: public slot_base, public slot<E>
        {
            slot_store( slot_store const & ) = delete;
            slot_store & operator=( slot_store const & ) = delete;
            void deactivate() noexcept final override
            {
                slot<E>::deactivate();
            }
            void propagate( int err_id ) noexcept final override
            {
                slot<E>::propagate(err_id);
            }
            void print( std::ostream & os, int key_to_print ) const final override
            {
                slot<E>::print(os, key_to_print);
            }
        public:
            template <class T>
            BOOST_LEAF_CONSTEXPR slot_store( int err_id, T && e )
            {
                slot<E>::put(err_id, std::forward<T>(e));
            }
        };

        slot_base * first_;
        slot_base * * last_;

    public:

        BOOST_LEAF_CONSTEXPR e_unexpected_info() noexcept:
            first_(nullptr),
            last_(&first_)
        {
        }

        BOOST_LEAF_CONSTEXPR e_unexpected_info( e_unexpected_info && other ) noexcept:
            first_(other.first_),
            last_(other.last_ == &other.first_? &first_ : other.last_)
        {
            BOOST_LEAF_ASSERT(last_ != nullptr);
            BOOST_LEAF_ASSERT(*last_ == nullptr);
            other.first_ = nullptr;
            other.last_ = nullptr;
        }

        ~e_unexpected_info() noexcept
        {
            for( slot_base const * p = first_; p; )
            {
                slot_base const * n = p -> next_;
                delete p;
                p = n;
            }
        }

        template <class E>
        BOOST_LEAF_CONSTEXPR typename std::decay<E>::type & put(int err_id, E && e)
        {
            using T = typename std::decay<E>::type;
            BOOST_LEAF_ASSERT(last_ != nullptr);
            BOOST_LEAF_ASSERT(*last_ == nullptr);
            BOOST_LEAF_ASSERT(tls::read_ptr<slot<T>>() == nullptr);
            slot_store<T> * ss = new slot_store<T>(err_id, std::forward<E>(e));
            *last_ = ss;
            last_ = &ss->next_;
            ss->activate();
            return ss->value(err_id);
        }

        void deactivate() noexcept
        {
            for( slot_base * p=first_; p; p=p->next_ )
                p->deactivate();
        }

        void propagate( int err_id ) noexcept
        {
            for( slot_base * p=first_; p; p=p->next_ )
                p->propagate(err_id);
        }

        template <class CharT, class Traits>
        void print( std::basic_ostream<CharT, Traits> & os, int key_to_print ) const
        {
            os << "Unhandled error objects:\n";
            for( slot_base const * p=first_; p; p=p->next_ )
                p->print(os, key_to_print);
        }
    };

    template <>
    struct diagnostic<e_unexpected_info, false, false>
    {
        static constexpr bool is_invisible = true;

        template <class CharT, class Traits>
        BOOST_LEAF_CONSTEXPR static void print( std::basic_ostream<CharT, Traits> &, e_unexpected_info const &) noexcept { }
    };

}

#endif

} }

////////////////////////////////////////

namespace boost { namespace leaf {

struct BOOST_LEAF_SYMBOL_VISIBLE e_source_location
{
    char const * file;
    int line;
    char const * function;

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, e_source_location const & x )
    {
        return os << leaf::type<e_source_location>() << ": " << x.file << '(' << x.line << ") in function " << x.function;
    }
};

////////////////////////////////////////

namespace leaf_detail
{
    template <class E>
    class BOOST_LEAF_SYMBOL_VISIBLE slot:
        optional<E>
    {
        slot( slot const & ) = delete;
        slot & operator=( slot const & ) = delete;

        using impl = optional<E>;
        slot<E> * prev_;

    public:

        BOOST_LEAF_CONSTEXPR slot() noexcept:
            prev_(nullptr)
        {
        }

        BOOST_LEAF_CONSTEXPR slot( slot && x ) noexcept:
            optional<E>(std::move(x)),
            prev_(nullptr)
        {
            BOOST_LEAF_ASSERT(x.prev_==nullptr);
        }

        void activate() noexcept
        {
            prev_ = tls::read_ptr<slot<E>>();
            tls::write_ptr<slot<E>>(this);
        }

        void deactivate() noexcept
        {
            tls::write_ptr<slot<E>>(prev_);
        }

        void propagate( int err_id ) noexcept(!BOOST_LEAF_CFG_DIAGNOSTICS);

        template <class CharT, class Traits>
        void print( std::basic_ostream<CharT, Traits> & os, int key_to_print ) const
        {
#if BOOST_LEAF_CFG_DIAGNOSTICS
            if( !diagnostic<E>::is_invisible )
                if( int k = this->key() )
                {
                    if( key_to_print )
                    {
                        if( key_to_print!=k )
                            return;
                    }
                    else
                        os << '[' << k << "] ";
                    diagnostic<E>::print(os, value(k));
                    (os << '\n').flush();
                }
#else
            (void) os;
            (void) key_to_print;
#endif
        }

        using impl::put;
        using impl::has_value;
        using impl::value;
    };

#if BOOST_LEAF_CFG_DIAGNOSTICS

    template <>
    inline void slot<e_unexpected_info>::deactivate() noexcept
    {
        if( int const err_id = this->key() )
            if( e_unexpected_info * info = this->has_value(err_id) )
                info->deactivate();
        tls::write_ptr<slot<e_unexpected_info>>(prev_);
    }

    template <>
    inline void slot<e_unexpected_info>::propagate( int err_id )
    {
        if( e_unexpected_info * info = this->has_value(err_id) )
            info->propagate(err_id);
    }

    template <class E>
    BOOST_LEAF_CONSTEXPR inline void load_unexpected_count( int err_id ) noexcept
    {
        if( slot<e_unexpected_count> * sl = tls::read_ptr<slot<e_unexpected_count>>() )
        {
            if( e_unexpected_count * unx = sl->has_value(err_id) )
                ++unx->count;
            else
                sl->put(err_id, e_unexpected_count(&type<E>));
        }
    }

    template <class E>
    BOOST_LEAF_CONSTEXPR inline void load_unexpected_info( int err_id, E && e )
    {
        if( slot<e_unexpected_info> * sl = tls::read_ptr<slot<e_unexpected_info>>() )
        {
            if( e_unexpected_info * unx = sl->has_value(err_id) )
                (void) unx->put(err_id, std::forward<E>(e));
            else
                (void) sl->put(err_id).put(err_id, std::forward<E>(e));
        }
    }

    template <class E, class F>
    BOOST_LEAF_CONSTEXPR inline void accumulate_unexpected_info( int err_id, F && f )
    {
        if( slot<e_unexpected_info> * sl = tls::read_ptr<slot<e_unexpected_info>>() )
        {
            if( e_unexpected_info * unx = sl->has_value(err_id) )
                (void) std::forward<F>(f)(unx->put(err_id, E{}));
            else
                (void) std::forward<F>(f)(sl->put(err_id).put(err_id, E{}));
        }
    }

    template <bool OnError, class E>
    inline void load_unexpected( int err_id, E && e  ) noexcept(OnError)
    {
        load_unexpected_count<E>(err_id);
        if( OnError )
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            try
            {
#endif
                load_unexpected_info(err_id, std::forward<E>(e));
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            }
            catch(...)
            {
            }
#endif
        }
        else
            load_unexpected_info(err_id, std::forward<E>(e));
    }

    template <bool OnError, class E, class F>
    inline void accumulate_unexpected( int err_id, F && f  ) noexcept(OnError)
    {
        load_unexpected_count<E>(err_id);
        if( OnError )
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            try
            {
#endif
                accumulate_unexpected_info<E>(err_id, std::forward<F>(f));
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            }
            catch(...)
            {
            }
#endif
        }
        else
            accumulate_unexpected_info<E>(err_id, std::forward<F>(f));
    }

#endif

    template <class E>
    inline void slot<E>::propagate( int err_id ) noexcept(!BOOST_LEAF_CFG_DIAGNOSTICS)
    {
        if( this->key()!=err_id && err_id!=0 )
            return;
        if( impl * p = tls::read_ptr<slot<E>>() )
            *p = std::move(*this);
#if BOOST_LEAF_CFG_DIAGNOSTICS
        else
        {
            int c = int(tls::read_uint<tls_tag_unexpected_enabled_counter>());
            BOOST_LEAF_ASSERT(c>=0);
            if( c )
                load_unexpected<false>(err_id, std::move(*this).value(err_id));
        }
#endif
    }

    template <bool OnError, class E>
    BOOST_LEAF_CONSTEXPR inline int load_slot( int err_id, E && e ) noexcept(OnError)
    {
        static_assert(!std::is_pointer<E>::value, "Error objects of pointer types are not allowed");
        static_assert(!std::is_same<typename std::decay<E>::type, error_id>::value, "Error objects of type error_id are not allowed");
        using T = typename std::decay<E>::type;
        BOOST_LEAF_ASSERT((err_id&3)==1);
        if( slot<T> * p = tls::read_ptr<slot<T>>() )
        {
            if( !OnError || !p->has_value(err_id) )
                (void) p->put(err_id, std::forward<E>(e));
        }
#if BOOST_LEAF_CFG_DIAGNOSTICS
        else
        {
            int c = int(tls::read_uint<tls_tag_unexpected_enabled_counter>());
            BOOST_LEAF_ASSERT(c>=0);
            if( c )
                load_unexpected<OnError>(err_id, std::forward<E>(e));
        }
#endif
        return 0;
    }

    template <bool OnError, class F>
    BOOST_LEAF_CONSTEXPR inline int accumulate_slot( int err_id, F && f ) noexcept(OnError)
    {
        static_assert(function_traits<F>::arity==1, "Lambdas passed to accumulate must take a single e-type argument by reference");
        using E = typename std::decay<fn_arg_type<F,0>>::type;
        static_assert(!std::is_pointer<E>::value, "Error objects of pointer types are not allowed");
        BOOST_LEAF_ASSERT((err_id&3)==1);
        if( auto sl = tls::read_ptr<slot<E>>() )
        {
            if( auto v = sl->has_value(err_id) )
                (void) std::forward<F>(f)(*v);
            else
                (void) std::forward<F>(f)(sl->put(err_id,E()));
        }
#if BOOST_LEAF_CFG_DIAGNOSTICS
        else
        {
            int c = int(tls::read_uint<tls_tag_unexpected_enabled_counter>());
            BOOST_LEAF_ASSERT(c>=0);
            if( c )
                accumulate_unexpected<OnError, E>(err_id, std::forward<F>(f));
        }
#endif
        return 0;
    }
}

////////////////////////////////////////

namespace leaf_detail
{
    template <class=void>
    struct BOOST_LEAF_SYMBOL_VISIBLE id_factory
    {
        static atomic_unsigned_int counter;

        BOOST_LEAF_CONSTEXPR static unsigned generate_next_id() noexcept
        {
            auto id = (counter+=4);
            BOOST_LEAF_ASSERT((id&3)==1);
            return id;
        }
    };

    template <class T>
    atomic_unsigned_int id_factory<T>::counter(unsigned(-3));

    inline int current_id() noexcept
    {
        unsigned id = tls::read_uint<tls_tag_id_factory_current_id>();
        BOOST_LEAF_ASSERT(id==0 || (id&3)==1);
        return int(id);
    }

    inline int new_id() noexcept
    {
        unsigned id = id_factory<>::generate_next_id();
        tls::write_uint<tls_tag_id_factory_current_id>(id);
        return int(id);
    }
}

////////////////////////////////////////

namespace leaf_detail
{
    template <class T, int Arity = function_traits<T>::arity>
    struct load_item
    {
        static_assert(Arity==0 || Arity==1, "If a functions is passed to new_error or load, it must take zero or one argument");
    };

    template <class E>
    struct load_item<E, -1>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, E && e ) noexcept
        {
            return load_slot<false>(err_id, std::forward<E>(e));
        }
    };

    template <class F>
    struct load_item<F, 0>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, F && f ) noexcept
        {
            return load_slot<false>(err_id, std::forward<F>(f)());
        }
    };

    template <class F>
    struct load_item<F, 1>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, F && f ) noexcept
        {
            return accumulate_slot<false>(err_id, std::forward<F>(f));
        }
    };
}

////////////////////////////////////////

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR

namespace leaf_detail
{
    class leaf_category final: public std::error_category
    {
        bool equivalent( int,  std::error_condition const & ) const noexcept final override { return false; }
        bool equivalent( std::error_code const &, int ) const noexcept final override { return false; }
        char const * name() const noexcept final override { return "LEAF error"; }
        std::string message( int ) const final override { return name(); }
    public:
        ~leaf_category() noexcept final override { }
    };

    template <class=void>
    struct get_error_category
    {
        static leaf_category cat;
    };

    template <class T>
    leaf_category get_error_category<T>::cat;

    inline int import_error_code( std::error_code const & ec ) noexcept
    {
        if( int err_id = ec.value() )
        {
            std::error_category const & cat = get_error_category<>::cat;
            if( &ec.category()==&cat )
            {
                BOOST_LEAF_ASSERT((err_id&3)==1);
                return (err_id&~3)|1;
            }
            else
            {
                err_id = new_id();
                (void) load_slot<false>(err_id, ec);
                return (err_id&~3)|1;
            }
        }
        else
            return 0;
    }
}

inline bool is_error_id( std::error_code const & ec ) noexcept
{
    bool res = (&ec.category() == &leaf_detail::get_error_category<>::cat);
    BOOST_LEAF_ASSERT(!res || !ec.value() || ((ec.value()&3)==1));
    return res;
}

#endif

////////////////////////////////////////

namespace leaf_detail
{
    BOOST_LEAF_CONSTEXPR error_id make_error_id(int) noexcept;
}

class BOOST_LEAF_SYMBOL_VISIBLE error_id
{
    friend error_id BOOST_LEAF_CONSTEXPR leaf_detail::make_error_id(int) noexcept;

    int value_;

    BOOST_LEAF_CONSTEXPR explicit error_id( int value ) noexcept:
        value_(value)
    {
        BOOST_LEAF_ASSERT(value_==0 || ((value_&3)==1));
    }

public:

    BOOST_LEAF_CONSTEXPR error_id() noexcept:
        value_(0)
    {
    }

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR
    error_id( std::error_code const & ec ) noexcept:
        value_(leaf_detail::import_error_code(ec))
    {
        BOOST_LEAF_ASSERT(!value_ || ((value_&3)==1));
    }

    template <class Enum>
    error_id( Enum e, typename std::enable_if<std::is_error_code_enum<Enum>::value, Enum>::type * = 0 ) noexcept:
        value_(leaf_detail::import_error_code(e))
    {
    }

    std::error_code to_error_code() const noexcept
    {
        return std::error_code(value_, leaf_detail::get_error_category<>::cat);
    }
#endif

    BOOST_LEAF_CONSTEXPR error_id load() const noexcept
    {
        return *this;
    }

    template <class Item>
    BOOST_LEAF_CONSTEXPR error_id load(Item && item) const noexcept
    {
        if (int err_id = value())
        {
            int const unused[] = { 42, leaf_detail::load_item<Item>::load_(err_id, std::forward<Item>(item)) };
            (void)unused;
        }
        return *this;
    }

    template <class... Item>
    BOOST_LEAF_CONSTEXPR error_id load( Item && ... item ) const noexcept
    {
        if( int err_id = value() )
        {
            int const unused[ ] = { 42, leaf_detail::load_item<Item>::load_(err_id, std::forward<Item>(item))... };
            (void) unused;
        }
        return *this;
    }

    BOOST_LEAF_CONSTEXPR int value() const noexcept
    {
        BOOST_LEAF_ASSERT(value_==0 || ((value_&3)==1));
        return value_;
    }

    BOOST_LEAF_CONSTEXPR explicit operator bool() const noexcept
    {
        return value_ != 0;
    }

    BOOST_LEAF_CONSTEXPR friend bool operator==( error_id a, error_id b ) noexcept
    {
        return a.value_ == b.value_;
    }

    BOOST_LEAF_CONSTEXPR friend bool operator!=( error_id a, error_id b ) noexcept
    {
        return !(a == b);
    }

    BOOST_LEAF_CONSTEXPR friend bool operator<( error_id a, error_id b ) noexcept
    {
        return a.value_ < b.value_;
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, error_id x )
    {
        return os << x.value_;
    }

    BOOST_LEAF_CONSTEXPR void load_source_location_( char const * file, int line, char const * function ) const noexcept
    {
        BOOST_LEAF_ASSERT(file&&*file);
        BOOST_LEAF_ASSERT(line>0);
        BOOST_LEAF_ASSERT(function&&*function);
        BOOST_LEAF_ASSERT(value_);
        (void) load(e_source_location {file,line,function});
    }
};

namespace leaf_detail
{
    BOOST_LEAF_CONSTEXPR inline error_id make_error_id( int err_id ) noexcept
    {
        BOOST_LEAF_ASSERT(err_id==0 || (err_id&3)==1);
        return error_id((err_id&~3)|1);
    }
}

inline error_id new_error() noexcept
{
    return leaf_detail::make_error_id(leaf_detail::new_id());
}

template <class... Item>
inline error_id new_error( Item && ... item ) noexcept
{
    return leaf_detail::make_error_id(leaf_detail::new_id()).load(std::forward<Item>(item)...);
}

inline error_id current_error() noexcept
{
    return leaf_detail::make_error_id(leaf_detail::current_id());
}

////////////////////////////////////////////

class polymorphic_context
{
protected:

    polymorphic_context() noexcept = default;
    ~polymorphic_context() noexcept = default;

public:

    virtual error_id propagate_captured_errors() noexcept = 0;
    virtual void activate() noexcept = 0;
    virtual void deactivate() noexcept = 0;
    virtual void propagate( error_id ) noexcept = 0;
    virtual bool is_active() const noexcept = 0;
    virtual void print( std::ostream & ) const { };
    error_id captured_id_;

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, polymorphic_context const & ctx )
    {
        ctx.print(os);
        return os;
    }
};

#if BOOST_LEAF_CFG_CAPTURE
using context_ptr = std::shared_ptr<polymorphic_context>;
#endif

////////////////////////////////////////////

template <class Ctx>
class context_activator
{
    context_activator( context_activator const & ) = delete;
    context_activator & operator=( context_activator const & ) = delete;

#if !defined(BOOST_LEAF_NO_EXCEPTIONS) && BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
    int const uncaught_exceptions_;
#endif
    Ctx * ctx_;

public:

    explicit BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE context_activator(Ctx & ctx) noexcept:
#if !defined(BOOST_LEAF_NO_EXCEPTIONS) && BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
        uncaught_exceptions_(std::uncaught_exceptions()),
#endif
        ctx_(ctx.is_active() ? nullptr : &ctx)
    {
        if( ctx_ )
            ctx_->activate();
    }

    BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE context_activator( context_activator && x ) noexcept:
#if !defined(BOOST_LEAF_NO_EXCEPTIONS) && BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
        uncaught_exceptions_(x.uncaught_exceptions_),
#endif
        ctx_(x.ctx_)
    {
        x.ctx_ = nullptr;
    }

    BOOST_LEAF_ALWAYS_INLINE ~context_activator() noexcept
    {
        if( ctx_ && ctx_->is_active() )
            ctx_->deactivate();
    }
};

template <class Ctx>
BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE context_activator<Ctx> activate_context(Ctx & ctx) noexcept
{
    return context_activator<Ctx>(ctx);
}

////////////////////////////////////////////

template <class R>
struct is_result_type: std::false_type
{
};

template <class R>
struct is_result_type<R const>: is_result_type<R>
{
};

} }

#endif
