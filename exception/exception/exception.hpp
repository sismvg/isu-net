#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <exception>
#include <stdexcept>

#include <cstdint>

#include <ex-config/platform_config.hpp>

#include <exception/exception_define_helper.hpp>

/*
	Ϊ�˷���ʹ�ã����еĺ���������Ƴɿ�������
*/

//����һ��int64_t�Ĵ�����룬���Ҵ��д�����Ϣ���쳣
DEFINE_HAVE_CODE_EXCEPTION(exception_with_code, std::exception);

//���û����ܵ�������쳣��ʾ�û���Ŀ���ʹ�÷�������
DEFINE_EXCEPTION(bad_usage, std::logic_error);

//�Ӳ��׳��쳣
#define NEVER_THROW throw()

//�Ӳ��׳����ز���ϵͳ�����[�ɻָ��쳣],���ڴ��дԽ��
#define NEVER_THROW_C

//�쳣��������Ϊδ����
#define UNKNOW_AT_THROW

//�쳣��������Ϊ�ж��壬����״̬���ܻ�ı�
#define USEABL_AT_THROW

//�쳣�������ǰ�ȫ��,�����ع���δ����ǰ��״̬
#define HARD_KEEP

//�ṩƽ̨��ص��쳣ת����C++����,���ṩ��һ��raise()���������ṩƽ̨�޹ص������׳�

class platform_exception
	:public std::exception
{
public:
	platform_exception(const char* msg = nullptr);

	//��������������׳�һ��C++�����쳣
	inline virtual void raise() throw()
	{}
};

#ifdef _WINDOWS

/*
����ƽ̨֮������쳣�Ľṹ������ͬ,����ֻ���ṩһ��any��exception code,
�Լ�
*/

#include <boost/any.hpp>

class recoverable_platform_exception
	:public platform_exception
{//������ʵ����
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