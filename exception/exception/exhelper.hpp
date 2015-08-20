#ifndef EX_HELPER_HPP
#define EX_HELPER_HPP

#include <exception/exception.hpp>

#ifdef _WINDOWS
#include <windows.h>
#endif

/*
	��������ϵͳ����ʧ�ܵĻ����׳��쳣�����쳣��������ʧ�ܴ���
	����ʲôҲ����
*/
template<class T>
void try_throw_last_error(const char* msg = nullptr)
{
#ifdef _WINDOWS
	if(GetLastError()!=0)
	{
		throw exception_with_code(msg ? 
		msg : "Some local call has error",error_code_mapping<T>(GetLastError()));
	}
#endif

#ifdef _LINUX
#endif
}

struct platform_exception_info
{
#ifdef _WINDOWS
	DWORD flag, argument_count;
	const ULONG_PTR* arguments;
#endif

#ifdef _LINUX
#endif
};

#ifdef _WINDOWS

#define _PLATFORM_TRY __try

#define _PLATFORM_CATCH(what) __except(what)

#endif
/*
	������ƽ̨��׼�����׳��Ų�,exp�б��뱣���˴����쳣�Ľṹ
*/
inline void platform_raise(const recoverable_platform_exception& exp)
{
#pragma warning(push)
#pragma warning(disable:4244)
#ifdef _WINDOWS
	const auto& info = exp.info<platform_exception_info>();
	RaiseException(exp.code(),
		info.flag, info.argument_count, info.arguments);
#endif
#pragma warning(pop)
}
#endif