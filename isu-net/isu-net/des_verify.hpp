#ifndef ISU_DES_VERIFY_HPP
#define ISU_DES_VERIFY_HPP

#include <isu-net/isu_net_lib.hpp>

class des_verify_ident
{};

class des_verify
{
public:
	typedef des_verify_ident verify_ident;

	template<class T>
	inline verify_ident generic_verify(const T& ident)
	{
		return verify_ident();
	}

	inline bool check_verify(const verify_ident&)
	{
		return true;
	}

	inline verify_ident reply_verify(const verify_ident)
	{
		return verify_ident();
	}
};

#endif