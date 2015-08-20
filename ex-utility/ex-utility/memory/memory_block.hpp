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

template<class Void>//MSG:嗯...don't worry
inline void advance_in(Void*& buffer, size_t& size, size_t adv)
{//一律当成非const,当然内部不会修改他
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
	返回两个内存块的头的距离的绝对值
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
	返回两个内存块头的距离，若rhs比lhs要更加靠0x0000.0000更近话
	则其值是负的.
	由于符号的问题，signed_memory_length可能丢失数据
*/
template<class L,class R>
signed_memory_length signed_distance(const L& lhs, const R& rhs)
{
	return static_cast<signed_memory_length>(lhs.buffer - rhs.buffer);
}

/*
	distance和signed_distance的合体，但不会丢失距离数据
	当lhs>rhs时 pair.first==false 反之为true
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
	返回lhs标注的内存块的尾部到rhs标注的内存块的头部的距离
*/
template<class L,class R>
memory_length distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return distance(lhs, rhs);
}

/*
	从内存块尾部到内存块头部的距离，其他同非from_tail函数
*/
template<class L,class R>
signed_memory_length signed_distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return signed_distance(lhs, rhs);
}

/*
	从内存块尾部到头部的距离，其他同非from_tail函数
*/
template<class L,class R>
std::pair<bool, memory_length> safe_signed_distance_from_tail(L lhs, const R& rhs)
{
	to_tail(lhs);
	return safe_signed_distance(lhs, rhs);
}

#endif