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
指定了这个模式后调用insert和push_back时
若slice_head指定的内存区越界则会崩溃，而不是重新分配内存
*/
static const size_t concrete = 0x1;

/*
要分片的数据上预留了头部数据
这个模式下slicer不会自己分配内存使用
构造中传入的memory参数必须在slicer死前必须存活.
并且该模式强制concrete
*/
static const size_t memory_with_head = 0x10 | concrete;

/*
slice_head中需要包含用户自定义的头部
指定了这个模式则memory_with_head中预留的头部
指定extern_head时必须指定generic
*/
static const size_t extern_head = 0x100;

/*
动态用户数据头,不允许和concrete同时存在
*/
static const size_t dynamic_head = 0x1000;

//slie_head的格式
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
	//无论如何,slice不会参数memory的创建/删除操作
	//slice永远是一个接配器
public:

	typedef slice self_type;
	slice();
	/*
		绑定到一块数据上,block在slice析构以前必须有效
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
	slicer的头部格式
	slicer_head-(slice_head-data)-(slice_head-data) rep.
*/

class slicer
{
	//动态模式会参与内存分配
	//静态模式会直接使用给予的内存
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
		memory:指向一块内存

		slize_size:分片的大小,包括头部
		mode:

		dynamic_alloc模式下表示memory并没有未slice_head预留内存
		并且由slicer删除

		static_alloc模式下表示预留了,那么便不会创建新的内存,
		而是直接在memory上直接分配

		*/
	slicer(const_memory_block memory, size_t slice_size,
		slice_mode mode = extern_head | concrete);
	/*
		gen指定了用户头的数据生成器
		user_keep在动态头时会被忽略
		其他不变
		*/
	slicer(const_memory_block memory, size_t slice_size,
		user_head_genericer gen, size_t user_keep,
		slice_mode mode = extern_head | concrete);

	/*
		slicer的头部
		*/
	const slicer_head& head();

	/*
		生成所有未生产的头部
	*/
	void generic_all();

	/*
		用于遍历slice,请在数据全部分片完成后使用
	*/
	const_iterator begin() const;
	const_iterator end() const;

	iterator begin();
	iterator end();
	/*
		分片数量,和随机访问支持
		*/
	size_t size() const;

	/*
		获取下一个未生产分片
	*/
	iterator get_next_slice();
	const bool slice_end() const;
	/*
		当前要分片的内存
		*/
	const_memory_block memory() const;

	/*
		分片大小
	*/

	size_t slice_size() const;

	/*
		计算要分片这些数据需要多少的内存来保存
		不允许任意flag的出现
		动态头不能使用这个函数
		*/
	static size_t compute_seralize_size(size_t data_size,
		size_t slice_size, size_t keep);

	static std::pair<size_t, size_t> how_many_slice(size_t data_size,
		size_t slice_size, size_t keep);
private:

	void _init(const_memory_block, size_t slice_size,
		user_head_genericer gen, size_t keep, slice_mode mode);

	void _check_argument_avaliable();

	//片的数量-1,最后一片的大小
	//片的大小包含数据


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
		懒惰生成的环境参数
	*/
	//已经使用掉的用户内存
	size_t _used_memory;
	//当前生成完的slice_id;
	size_t _current_slice_id;

	iterator _generic();
	//slice_count从1开始
	//id从0开始
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