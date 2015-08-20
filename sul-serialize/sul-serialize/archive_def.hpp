#ifndef ISU_ARCHIVE_DEF_HPP
#define ISU_ARCHIVE_DEF_HPP

#include <utility>
#include <xstddef>
#include <iterator>
#include <type_traits>

#include <boost/mpl/if.hpp>
#include <ex-utility/memory/memory_block.hpp>

typedef std::true_type true_type;
typedef std::false_type false_type;
extern int ptrv;
template<class T>
struct real_type
{
	typedef typename std::remove_reference<T>::type type;
};

template<class T>
class iter_wapper
{
public:
	typedef T type;
};

template<class T, std::size_t Size>
class iter_wapper < T[Size] >
{
public:
	typedef T* type;
};

template<class T>
class iterator_value_type
{
public:
	typedef typename
		std::iterator_traits<T>::value_type type;
};

template<template<class> class WIter, class Con>
class iterator_value_type < WIter<Con> >
{
public:
	typedef typename Con::value_type type;
};


class vector_air{};

template<class T>
class is_air
{
public:
	static const bool value = std::is_same<T, vector_air>::value;
};

typedef long long long64;

//������Щ����������̫�ࡣ������ֱ�����˸���
#define MYIF boost::mpl::if_

template<size_t value>
using constant = std::integral_constant < size_t, value > ;

template<class T1,class T2>
using mysame = std::is_same < T1, T2 > ;

#define NOT_TYPE(tp) typename mysame<T, tp >::type;

template<class T>
struct is_noptype
{
	static const bool not_char = !mysame<T, char>::type::value,
		not_uchar = !mysame<T, unsigned char>::type::value,
		not_bool = !mysame<T, bool>::type::value,
		_1bit = mysame < constant<sizeof(T)>,
		constant < 1 >> ::type::value;

	static const bool value =
		not_char&&not_uchar&&not_bool&&_1bit;

	typedef std::integral_constant<bool, value> type;
};

template<class T>
struct is_multiple_type_impl
{
	static const bool value = false;
};


template<class T>
struct is_multiple_type
{
	static const bool value =
		is_multiple_type_impl<T>::value;
};

template<class T>
struct is_diamond_type
{
	static const bool value = false;
};

typedef constant<0> pod_type;
typedef constant<1> nop_type;
typedef constant<2> multiple_type;
typedef constant<3> diamond_type;
typedef constant<4> normal_type;
typedef constant<5> reference_type;
template<class T>
struct detail_of_type_impl
{
	//����д��ô����װ�ˣ�ֱ��ȫ����myifд��������
	typedef typename
		MYIF<typename std::is_pod<T>::type,
			pod_type, normal_type>::type t1;

	typedef typename
		MYIF<typename is_noptype<T>::type,
			nop_type, t1>::type type;
};
template<class T>
struct detail_of_type_impl < T& >
{
	typedef reference_type type;
};

template<class T>
struct detail_of_type
{//�жϸ����͵�������Ϣ��PODɶ�ģ�
	typedef typename detail_of_type_impl<T>::type type;
};

template<class T>
struct constant_length
{
	typedef typename detail_of_type_impl<T>::type type_detail;
	static const size_t tmp = type_detail::value;
	static const bool value = (
		tmp == pod_type::value ||
		tmp == normal_type::value ||
		tmp == nop_type::value);
};

typedef std::true_type true_type;
typedef std::false_type false_type;

#define MPL_VEC boost::mpl::vector
#define MPL_PUSH boost::mpl::push_back
#define MPL_INSERT boost::mpl::insert
#define MPL_IF boost::mpl::if_

#ifndef NOP
#define NOP
#endif

#define CV_INCERMENT(cv)\
	cv char* cptr = reinterpret_cast<cv char*>(ptr);\
	size_t size = (BitCnt*loop_count);\
	cptr += size;\
	ptr=cptr;\
	return size

template<size_t BitCnt,class Ptr>
size_t incerment(Ptr*& ptr, size_t loop_count)
{
	CV_INCERMENT(NOP);
}

template<size_t BitCnt,class Ptr>
size_t incerment(const Ptr*& ptr, size_t loop_count)
{
	CV_INCERMENT(const);
}

/*
	TODO:serializeѰ�Һ�����ʱ�����ر�xdr_filterҪ��
*/
memory_length archived_size(const const_memory_block& blk);
memory_length archived_size(const memory_block& blk);

memory_length archive_to_with_size(memory_address buffer,
memory_length bufsize, const const_memory_block&, memory_length size);

memory_length archive_to_with_size(memory_address buffer,
memory_length bufsize, const memory_block& blk, memory_length size);

memory_length rarchive(const_memory_block src, const_memory_block& blk);
memory_length rarchive(const_memory_block src, memory_block& blk);
#endif