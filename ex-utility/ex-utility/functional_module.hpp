#ifndef ISU_FUNCTIONAL_MODULE_HPP
#define ISU_FUNCTIONAL_MODULE_HPP

#include <ex-utility/ex_utility_lib.hpp>

/*
	һ������.ȫ��Ϊ-������ģ��
	���Ե���dll�ĳ���
*/

class functional_module
{
	/*
		����ֵ����,����ʱ�󶨺ʹ���
	*/
	template<class Nick,class... Arg>
	void call(const Nick&, Arg... args);
};
#endif