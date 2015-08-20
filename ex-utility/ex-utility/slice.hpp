#ifndef ISU_SLICE_HPP
#define ISU_SLICE_HPP

#include <list>
#include <cstddef>
#include <hash_set>
#include <functional>

#include <ex-utility/ex_utility_lib.hpp>

#include <ex-utility/memory/memory_helper.hpp>
#include <sul-serialize/archive_packages.hpp>

typedef std::size_t slice_mode;

/*
ָ�������ģʽ�����insert��push_backʱ
��slice_headָ�����ڴ���Խ�������������������·����ڴ�
*/
static const size_t concrete = 0x1;

/*
Ҫ��Ƭ��������Ԥ����ͷ������
���ģʽ��slicer�����Լ������ڴ�ʹ��
�����д����memory����������slicer��ǰ������.
���Ҹ�ģʽǿ��concrete
*/
static const size_t memory_with_head = 0x10 | concrete;

/*
slice_head����Ҫ�����û��Զ����ͷ��
ָ�������ģʽ��memory_with_head��Ԥ����ͷ��
ָ��extern_headʱ����ָ��generic
*/
static const size_t extern_head = 0x100;

/*
��̬�û�����ͷ,�������concreteͬʱ����
*/
static const size_t dynamic_head = 0x1000;

//slie_head�ĸ�ʽ
//head_size-slice_head-user_headhead

//slice_head-user_head
struct slice_head
{
	typedef std::size_t size_t;
	size_t slice_size;
	size_t slice_id;
	size_t start;
#ifdef _USER_DEBUG
	size_t debug_crc;
#endif
};


class slice
{
	//�������,slice�������memory�Ĵ���/ɾ������
	//slice��Զ��һ��������
public:

	typedef slice self_type;
	slice();
	/*
		�󶨵�һ��������,block��slice������ǰ������Ч
	*/
	slice(const_memory_block block);

	typedef slice_head::size_t size_t;

	/*
		slice memory size
	*/
	size_t size() const;

	/*
		memory
	*/
	const_memory_block memory() const;

	/*
		memory without head
	*/
	const_memory_block data() const;

private:

	void _init(const_memory_block block);

	const_memory_block _memory;
};

struct slicer_head
{
	size_t data_size;
};

/*
	slicer��ͷ����ʽ
	slicer_head-(slice_head-data)-(slice_head-data) rep.
*/

class slicer
{
	//��̬ģʽ������ڴ����
	//��̬ģʽ��ֱ��ʹ�ø�����ڴ�
public:

	typedef char bit;
	typedef const char const_bit;
	typedef slice_head::size_t size_t;

	typedef shared_memory_block<bit> const_shared_memory;

	typedef std::list<std::pair<const_shared_memory, slice>> impl;

	typedef impl::value_type value_type;
	typedef impl::iterator iterator;
	typedef impl::const_iterator const_iterator;

	//slice_id,slice_start,pac
	typedef std::function <void(size_t,size_t,
		archive_packages&) > user_head_genericer;

	slicer();
	/*
		memory:ָ��һ���ڴ�

		slize_size:��Ƭ�Ĵ�С,����ͷ��
		mode:

		dynamic_allocģʽ�±�ʾmemory��û��δslice_headԤ���ڴ�
		������slicerɾ��

		static_allocģʽ�±�ʾԤ����,��ô�㲻�ᴴ���µ��ڴ�,
		����ֱ����memory��ֱ�ӷ���

		*/
	slicer(const_memory_block memory, size_t slice_size,
		slice_mode mode = extern_head | concrete);
	/*
		genָ�����û�ͷ������������
		user_keep�ڶ�̬ͷʱ�ᱻ����
		��������
		*/
	slicer(const_memory_block memory, size_t slice_size,
		user_head_genericer gen, size_t user_keep,
		slice_mode mode = extern_head | concrete);

	/*
		slicer��ͷ��
		*/
	const slicer_head& head();

	/*
		��������δ������ͷ��
	*/
	void generic_all();

	/*
		���ڱ���slice,��������ȫ����Ƭ��ɺ�ʹ��
	*/
	const_iterator begin() const;
	const_iterator end() const;

	iterator begin();
	iterator end();
	/*
		��Ƭ����,���������֧��
		*/
	size_t size() const;

	/*
		��ȡ��һ��δ������Ƭ
	*/
	iterator get_next_slice();
	const bool slice_end() const;
	/*
		��ǰҪ��Ƭ���ڴ�
		*/
	const_memory_block memory() const;

	/*
		��Ƭ��С
	*/

	size_t slice_size() const;

	/*
		����Ҫ��Ƭ��Щ������Ҫ���ٵ��ڴ�������
		����������flag�ĳ���
		��̬ͷ����ʹ���������
		*/
	static size_t compute_seralize_size(size_t data_size,
		size_t slice_size, size_t keep);

	static std::pair<size_t, size_t> how_many_slice(size_t data_size,
		size_t slice_size, size_t keep);
private:

	void _init(const_memory_block, size_t slice_size,
		user_head_genericer gen, size_t keep, slice_mode mode);

	void _check_argument_avaliable();

	//Ƭ������-1,���һƬ�Ĵ�С
	//Ƭ�Ĵ�С��������


	void _slice();

	void _extern_memory(size_t size);
	void _init_static_memory(const_memory_block);

	const_memory_block _alloc_slice_data(size_t start, size_t size);

	slice_mode _mode;

	size_t _slice_size;
	size_t _user_keep;

	slicer_head _vector_head;

	user_head_genericer _gen;

	const_memory_block _memory;
	impl _slices;

	/*
		�������ɵĻ�������
	*/
	//�Ѿ�ʹ�õ����û��ڴ�
	size_t _used_memory;
	//��ǰ�������slice_id;
	size_t _current_slice_id;

	iterator _generic();
	//slice_count��1��ʼ
	//id��0��ʼ
	void _generic_to(size_t slice_count);
};

class combination
{
public:

	typedef char bit;
	typedef slice::size_t size_t;
	typedef shared_memory_block<char> shared_memory;
	/*
		head mark the how to sliced.
		and pro is user define to eat user head
	*/
	combination();
	combination(const slicer_head& head);

	/*
		Insert data without head to memory,
	*/
	void insert(const std::pair<slice_head,slice>&);

	void insert(size_t slice_start, const const_memory_block& data);
	/*
		Return avaliable data
		pair.first=memory
		pair.second=avaliable memory size
		If slicer is dynamic_head mode.
		until all slice accepted.pair.second is zero
	*/
	std::pair<shared_memory, size_t> data() const;

	/*
		check is all accepted
	*/
	bool accepted_all() const;

	/*
		How many slice accepted
	*/
	size_t accepted() const;
private:
	void _init(const slicer_head&);
	/*
		_slices.size() not accepted
	*/
	std::hash_set<size_t> _recved;

	size_t _avaliable_memory;
	shared_memory _memory;

	slicer_head _head;

	//impl

	memory_block _get_range(size_t start,
		const const_memory_block& data);
};
#endif