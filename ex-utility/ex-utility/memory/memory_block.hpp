#ifndef MEMORY_BLOCK_HPP
#define MEMORY_BLOCK_HPP

#include <cstddef>
#include <ex-config/platform_config.hpp>
typedef std::size_t memory_length;
typedef long long signed_memory_length;

typedef void* memory_address;
typedef const void* const_memory_address;

#include <utility>

struct memory_block
{
	memory_block()
		:buffer(nullptr), size(0)
	{}

	memory_block(memory_address addr, memory_length sz)
		:buffer(addr), size(sz)
	{}

	memory_address buffer;
	memory_length size;
};

struct const_memory_block
{
	const_memory_block()
		:buffer(nullptr), size(0)
	{}

	const_memory_block(const_memory_address addr, memory_length sz)
		:buffer(addr), size(sz)
	{}

	const_memory_block(const memory_block& blk)
		:buffer(blk.buffer), size(blk.size)
	{}

	const_memory_address buffer;
	memory_length size;
};

template<class Void>//MSG:��...don't worry
inline void advance_in(Void*& buffer, size_t& size, size_t adv)
{//һ�ɵ��ɷ�const,��Ȼ�ڲ������޸���
	const auto* tmp = reinterpret_cast<const char*>(buffer);
	tmp += adv;
#ifdef _DEBUG
	if (adv > size)
		throw std::length_error("advance_in adv"\
		"have to equal less to size");
#endif
	size -= adv;
	buffer = reinterpret_cast<Void*>(const_cast<char*>(tmp));
}

template<class MemBlk>
MemBlk advance(const MemBlk& blk, memory_length adv)
{
	MemBlk ret = blk;
	advance_in(ret, adv);
	return ret;
}

template<class MemBlk>
void advance_in(MemBlk& blk, memory_length adv)
{
	advance_in(blk.buffer, blk.size, adv);
}

#define ADVANCE_BLK(name) name##emory_block blk;\
	blk.buffer=buffer;\
	blk.size=size;\
	advance_in(blk,adv);\
	return blk

inline memory_block
advance(void* buffer, memory_length size, memory_length adv)
{
	ADVANCE_BLK(m);
}

inline const_memory_block
advance(const void* buffer, memory_length size, memory_length adv)
{
	ADVANCE_BLK(const_m);
}

/*
	���������ڴ���ͷ�ľ���ľ���ֵ
*/
template<class L,class R>
memory_length distance(const L& lhs, const R& rhs)
{
	auto _1 = lhs.buffer, _2 = rhs.buffer;
	if (_1 < _2)
		std::swap(_1, _2);
	return static_cast<memory_length>(_1 - _2);
}

/*
	���������ڴ��ͷ�ľ��룬��rhs��lhsҪ���ӿ�0x0000.0000������
	����ֵ�Ǹ���.
	���ڷ��ŵ����⣬signed_memory_length���ܶ�ʧ����
*/
template<class L,class R>
signed_memory_length signed_distance(const L& lhs, const R& rhs)
{
	return static_cast<signed_memory_length>(lhs.buffer - rhs.buffer);
}

/*
	distance��signed_distance�ĺ��壬�����ᶪʧ��������
	��lhs>rhsʱ pair.first==false ��֮Ϊtrue
*/
template<class L,class R>
std::pair<bool, memory_length> safe_signed_distance(const L& lhs, const R& rhs)
{
	return std::make_pair(lhs.buffer < rhs.buffer, distance(lhs, rhs));
}

template<class T>
void to_tail(T& block)
{
	advance_in(block, block.size);
}

/*
	����lhs��ע���ڴ���β����rhs��ע���ڴ���ͷ���ľ���
*/
template<class L,class R>
memory_length distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return distance(lhs, rhs);
}

/*
	���ڴ��β�����ڴ��ͷ���ľ��룬����ͬ��from_tail����
*/
template<class L,class R>
signed_memory_length signed_distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return signed_distance(lhs, rhs);
}

/*
	���ڴ��β����ͷ���ľ��룬����ͬ��from_tail����
*/
template<class L,class R>
std::pair<bool, memory_length> safe_signed_distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return safe_signed_distance(lhs, rhs);
}

#endif