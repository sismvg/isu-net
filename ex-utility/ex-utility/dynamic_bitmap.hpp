#ifndef ISU_DYNAMIC_BITMAP_HPP
#define ISU_DYNAMIC_BITMAP_HPP

#include <ex-utility/ex_utility_lib.hpp>

#include <boost/dynamic_bitset.hpp>

class dynamic_bitmap
{
public:
	typedef std::size_t size_t;

	/*
		reserve:Ԥ����λ����
	*/
	dynamic_bitmap(size_t reserve = 0);

	/*
		����pos��ֵ
	*/
	bool test(size_t pos);
	/*
		���test(pos)Ϊ0���������posΪ1Ȼ��retrun false
		����return true
	*/
	bool test_and_set(size_t pos);

	/*
		����չ�ڴ��test_and_set
	*/
	bool just_test_and_set(size_t pos);

	/*
		����posΪ1,������ԭ����ֵ
	*/
	bool set(size_t pos, bool val);

	/*
		��������һ��val
	*/
	void push_back(bool val);

	/*
		�Ƿ������bit,���bit�Ƿ�����Ϊ1
	*/
	bool have(size_t pos) const;

	/*
		������Ƿ������bit,ע��,pos�������Ϊ0
	*/
	bool have_bit(size_t pos) const;
	/*
		����չ�ڴ��С��test��set
	*/
	bool just_test(size_t pos) const;
	bool just_set(size_t pos, bool val);
	/*
		�ж��ٸ�λ
	*/
	size_t size() const;
	size_t count() const;
	/*
		ȥ��Ԥ����size.
	*/
	size_t arrange();
private:

	void _try_extern(size_t size);
	boost::dynamic_bitset<unsigned long> _set;
};
#endif