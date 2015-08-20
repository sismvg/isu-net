#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <exception>
#include <stdexcept>

#include <cstdint>

#include <ex-config/platform_config.hpp>

#include <exception/exception_define_helper.hpp>

/*
	为了方便使用，所有的函数都被设计成可内联的
*/

//带有一个int64_t的错误代码，并且带有错误消息的异常
DEFINE_HAVE_CODE_EXCEPTION(exception_with_code, std::exception);

//当用户接受到了这个异常表示用户对目标的使用方法错了
DEFINE_EXCEPTION(bad_usage, std::logic_error);

//从不抛出异常
#define NEVER_THROW throw()

//从不抛出本地操作系统定义的[可恢复异常],如内存读写越界
#define NEVER_THROW_C

//异常发生后行为未定义
#define UNKNOW_AT_THROW

//异常发生后行为有定义，但是状态可能会改变
#define USEABL_AT_THROW

//异常发生后是安全的,对象会回滚到未调用前的状态
#define HARD_KEEP

//提供平台相关的异常转换到C++风格的,并提供了一个raise()函数用于提供平台无关的重新抛出

class platform_exception
	:public std::exception
{
public:
	platform_exception(const char* msg = nullptr);

	//这个函数绝不会抛出一个C++风格的异常
	inline virtual void raise() throw()
	{}
};

#ifdef _WINDOWS

/*
由于平台之间各个异常的结构并不相同,所以只能提供一个any和exception code,
以及
*/

#include <boost/any.hpp>

class recoverable_platform_exception
	:public platform_exception
{//还不能实例化
public:
	recoverable_platform_exception()
		:platform_exception(nullptr)
	{}

	template<class T>
	recoverable_platform_exception(const char* msg,const T& info,int64_t code)
		:_exception_info(info),platform_exception(msg),_code(code)
	{}

	template<class T>
	T info() const
	{
		return boost::any_cast<T>(_exception_info);
	}

	int64_t code() const
	{
		return _code;
	}
private:
	boost::any _exception_info;
	int64_t _code;
};

#endif

#ifdef _LINUX
#endif

#endif