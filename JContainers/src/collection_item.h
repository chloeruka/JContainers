#pragma once

#include <boost/variant.hpp>
#include <string>

#include "object/object_base.h"
#include "skse.h"
#include "common/ITypes.h"

#include "collections.h"
#include "collection_types.h"

namespace collections {

    enum item_type {
        no_item = 0,
        none,
        integer,
        real,
        form,
        object,
        string,
    };

    class Item {
    public:
        typedef boost::blank blank;
        typedef Float32 Real;
        typedef boost::variant<boost::blank, SInt32, Real, FormId, internal_object_ref, std::string> variant;

    private:
        variant _var;

    private:

        template<class T> struct type2index{ };

        template<> struct type2index < boost::blank >  { static const item_type index = none; };
        template<> struct type2index < SInt32 >  { static const item_type index = integer; };
        template<> struct type2index < Real >  { static const item_type index = real; };
        template<> struct type2index < FormId >  { static const item_type index = form; };
        template<> struct type2index < internal_object_ref >  { static const item_type index = object; };
        template<> struct type2index < std::string >  { static const item_type index = string; };

        static_assert(type2index<Real>::index > type2index<SInt32>::index, "Item::type2index works incorrectly");

    public:

        void u_nullifyObject() {
            if (auto ref = boost::get<internal_object_ref>(&_var)) {
                ref->jc_nullify();
            }
        }

        Item() {}
        Item(Item&& other) : _var(std::move(other._var)) {}
        Item(const Item& other) : _var(other._var) {}

        Item& operator = (Item&& other) {
            _var = std::move(other._var);
            return *this;
        }

        Item& operator = (const Item& other) {
            _var = other._var;
            return *this;
        }

        const variant& var() const {
            return _var;
        }

        variant& var() {
            return _var;
        }

        template<class T> bool is_type() const {
            return boost::get<T>(&_var) != nullptr;
        }

        item_type type() const {
            return item_type(_var.which() + 1);
        }

        template<class T> T* get() {
            return boost::get<T>(&_var);
        }

        template<class T> const T* get() const {
            return boost::get<T>(&_var);
        }

        //////////////////////////////////////////////////////////////////////////

        friend class boost::serialization::access;
        BOOST_SERIALIZATION_SPLIT_MEMBER();

        template<class Archive>
        void save(Archive & ar, const unsigned int version) const;
        template<class Archive>
        void load(Archive & ar, const unsigned int version);

        //////////////////////////////////////////////////////////////////////////


        explicit Item(Real val) : _var(val) {}
        explicit Item(double val) : _var((Real)val) {}
        explicit Item(SInt32 val) : _var(val) {}
        explicit Item(int val) : _var((SInt32)val) {}
        explicit Item(bool val) : _var((SInt32)val) {}
        explicit Item(FormId id) : _var(id) {}
        explicit Item(object_base& o) : _var(o) {}

        explicit Item(const std::string& val) : _var(val) {}
        explicit Item(std::string&& val) : _var(val) {}

        // the Item is none if the pointers below are zero:
        explicit Item(const TESForm *val) {
            *this = val;
        }
        explicit Item(const char * val) {
            *this = val;
        }
        explicit Item(const BSFixedString& val) {
            *this = val.data;
        }
        explicit Item(object_base *val) {
            *this = val;
        }
        explicit Item(const object_stack_ref &val) {
            *this = val.get();
        }
        /*
        explicit Item(const BSFixedString& val) : _var(boost::blank()) {
        *this = val.data;
        }*/

        Item& operator = (unsigned int val) { _var = (SInt32)val; return *this; }
        Item& operator = (int val) { _var = (SInt32)val; return *this; }
        Item& operator = (bool val) { _var = (SInt32)val; return *this; }
        Item& operator = (SInt32 val) { _var = val; return *this; }
        Item& operator = (Real val) { _var = val; return *this; }
        Item& operator = (double val) { _var = (Real)val; return *this; }
        Item& operator = (const std::string& val) { _var = val; return *this; }
        Item& operator = (std::string&& val) { _var = val; return *this; }
        Item& operator = (boost::blank) { _var = boost::blank(); return *this; }
        Item& operator = (boost::none_t) { _var = boost::blank(); return *this; }
        Item& operator = (object_base& v) { _var = &v; return *this; }


        Item& operator = (FormId formId) {
            // prevent zero FormId from being saved
            if (formId) {
                _var = formId;
            }
            else {
                _var = blank();
            }
            return *this;
        }

        template<class T>
        Item& _assignPtr(T *ptr) {
            if (ptr) {
                _var = ptr;
            }
            else {
                _var = blank();
            }
            return *this;
        }

        Item& operator = (const char *val) {
            return _assignPtr(val);
        }

        Item& operator = (object_base *val) {
            return _assignPtr(val);
        }

        Item& operator = (const TESForm *val) {
            if (val) {
                _var = (FormId)val->formID;
            }
            else {
                _var = blank();
            }
            return *this;
        }

        object_base *object() const {
            if (auto ref = boost::get<internal_object_ref>(&_var)) {
                return ref->get();
            }
            return nullptr;
        }

        Real fltValue() const {
            if (auto val = boost::get<Item::Real>(&_var)) {
                return *val;
            }
            else if (auto val = boost::get<SInt32>(&_var)) {
                return *val;
            }
            return 0;
        }

        SInt32 intValue() const {
            if (auto val = boost::get<SInt32>(&_var)) {
                return *val;
            }
            else if (auto val = boost::get<Item::Real>(&_var)) {
                return *val;
            }
            return 0;
        }

        const char * strValue() const {
            if (auto val = boost::get<std::string>(&_var)) {
                return val->c_str();
            }
            return nullptr;
        }

        std::string * stringValue() {
            return boost::get<std::string>(&_var);
        }

        TESForm * form() const {
            auto frmId = formId();
            return frmId != FormZero ? skse::lookup_form(frmId) : nullptr;
        }

        FormId formId() const {
            if (auto val = boost::get<FormId>(&_var)) {
                return *val;
            }
            return FormZero;
        }

        class are_strict_equals : public boost::static_visitor<bool> {
        public:

            template <typename T, typename U>
            bool operator()(const T &, const U &) const {
                return false; // cannot compare different types
            }

            template <typename T>
            bool operator()(const T & lhs, const T & rhs) const {
                return lhs == rhs;
            }

            template <> bool operator()(const std::string & lhs, const std::string & rhs) const {
                return _stricmp(lhs.c_str(), rhs.c_str()) == 0;
            }
        };

        bool isEqual(const Item& other) const {
            return boost::apply_visitor(are_strict_equals(), _var, other._var);
        }

        bool isNull() const {
            return is_type<boost::blank>();
        }

        bool isNumber() const {
            return is_type<SInt32>() || is_type<Real>();
        }

        template<class T> T readAs();

        //////////////////////////////////////////////////////////////////////////
    private:
        static_assert(std::is_same<
            boost::variant<boost::blank, SInt32, Real, FormId, internal_object_ref, std::string>,
            variant
        >::value, "update _user2variant code below");

        // maps input user type to variant type:
        template<class T> struct _user2variant { using variant_type = T; };
        template<class V> struct _variant_type { using variant_type = V; };

        template<> struct _user2variant<uint32_t> : _variant_type<SInt32>{};
        template<> struct _user2variant<int32_t> : _variant_type<SInt32>{};
        template<> struct _user2variant<bool> : _variant_type<SInt32>{};

        template<> struct _user2variant<double> : _variant_type<Real>{};

        template<> struct _user2variant<const char*> : _variant_type<std::string>{};

        //template<> struct _user2variant<object_base> : _variant_type<internal_object_ref>{};
        template<> struct _user2variant<const object_base*> : _variant_type<internal_object_ref>{};

    public:

        bool operator == (const Item& other) const { return isEqual(other); }
        bool operator != (const Item& other) const { return !isEqual(other); }

        bool operator == (const object_base &obj) const { return *this == &obj; }

        template<class T>
        bool operator == (const T& v) const {
            auto thisV = boost::get<typename _user2variant<T>::variant_type >(&_var);
            return thisV && *thisV == v;
        }

        template<class T>
        bool operator != (const T& v) const { return !(*this == v); }

        //////////////////////////////////////////////////////////////////////////

        bool operator < (const Item& other) const {
            const auto l = type(), r = other.type();
            return l == r ? boost::apply_visitor(lesser_comparison(), _var, other._var) : (l < r);
        }
    private:
        class lesser_comparison : public boost::static_visitor < bool > {
        public:

            template <typename T, typename U>
            bool operator()(const T &, const U &) const {
                return type2index<T>::index < type2index<U>::index;
            }

            template <typename T>
            bool operator()(const T & lhs, const T & rhs) const {
                return lhs < rhs;
            }

            bool operator()(const std::string & lhs, const std::string & rhs) const {
                return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
            }

        };

    };

    template<> inline Item::Real Item::readAs<Item::Real>() {
        return fltValue();
    }

    template<> inline SInt32 Item::readAs<SInt32>() {
        return intValue();
    }

    template<> inline const char * Item::readAs<const char *>() {
        return strValue();
    }

    template<> inline std::string Item::readAs<std::string>() {
        auto str = stringValue();
        return str ? *str : std::string();
    }

    template<> inline BSFixedString Item::readAs<BSFixedString>() {
        const char *chr = strValue();
        return chr ? BSFixedString(chr) : BSFixedString();
    }

    template<> inline Handle Item::readAs<Handle>() {
        auto obj = object();
        return obj ? obj->uid() : HandleNull;
    }

    template<> inline TESForm * Item::readAs<TESForm*>() {
        return form();
    }

    template<> inline object_base * Item::readAs<object_base*>() {
        return object();
    }
}