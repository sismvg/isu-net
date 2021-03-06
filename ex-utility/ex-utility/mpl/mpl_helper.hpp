#ifndef ISU_MPL_HELPER_HPP
#define ISU_MPL_HELPER_HPP

#include <functional>
#include <type_traits>

template<class Sptr, class T, class R, class... Arg>
auto mem_fn_object(Sptr ptr, R(T::*mem_ptr)(Arg...))
->std::function < R(Arg...) >
{
	return[=](Arg... arg)
	{
		return ((*ptr).*mem_ptr)(arg...);
	};
}

#define MEMBER_BIND(obj,fn) (mem_fn_object(obj,\
	(&std::remove_reference<decltype(*obj)>::type::fn)))

#define MEMBER_BIND_IN(fn) MEMBER_BIND(this,fn)

#define VOID_TYPE_CALL(type) reinterpret_cast<type*>(nullptr)

#define VOID_TYPE_CALL_CONST(type) VOID_TYPE_CALL(const type)

template<class T>
struct is_no_need_copy
{
	typedef typename std::remove_cv<T>::type raw_type;
	typedef typename
		std::remove_pointer < typename
		std::remove_reference<T>::type > ::type cvtype;
	static const bool is_consted = std::is_const<cvtype>::value;
	static const bool is_local_var =
		!std::is_reference<raw_type>::value&&
		!std::is_pointer < raw_type >::value;
	static const bool value = is_consted || is_local_var;
	typedef std::integral_constant<bool, value> type;
};

#endif