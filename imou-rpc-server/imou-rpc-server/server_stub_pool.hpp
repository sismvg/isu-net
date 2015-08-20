#ifndef ISU_SERVER_STUB_POOL_HPP
#define ISU_SERVER_STUB_POOL_HPP

#include <set>
#include <atomic>
#include <hash_map>

#include <boost/smart_ptr/detail/spinlock.hpp>

#include <imou-rpc-kernel/stub_ident.hpp>
#include <imou-rpc-server/imouto_server_stub.hpp>

//每一个progid都需要一个池,所以就不需要传入一个progid用于构造了
//TODO-IMPL.实现可以更好点,比如不用都记录一个progid啥的

enum find_stat{
	find_success=0,
	prog_unknow ,
	proc_mismatch,
	ver_mismatch
};

template<class Map=std::hash_map<xdr_uint32,basic_server_stub<stub_ident>*>>
class stub_pool
{
public:
	typedef stub_ident ident_type;
	typedef basic_server_stub<ident_type> server_stub;
	typedef std::pair<stub_ident,server_stub*> value_type;
private:
	typedef std::hash_map < xdr_uint32, basic_server_stub<stub_ident>* > impl;
public:
	typedef std::size_t size_type;
	typedef typename impl::iterator iterator;
	typedef typename impl::const_iterator const_iterator;
	typedef std::pair<iterator,bool> register_result;
	typedef xdr_uint32 progid_type;
	typedef stub_pool self_type;

	stub_pool(size_type prog,size_type ver)
		:
		_spinlock(BOOST_DETAIL_SPINLOCK_INIT),
		_prog(prog), _ver(ver)
	{}

	~stub_pool()
	{
		//WAR.必须 保证不会有访问
		for (auto& value : _stubs)
			delete value.second;
	}

	progid_type prog_id() const
	{
		return _prog;
	}

	std::size_t ver() const
	{
		return _ver;
	}

	std::pair<const_iterator,find_stat>
		find(const ident_type& key) const
	{
		std::pair<const_iterator, find_stat> result;

		if (_prog != key.prog)
			result.second = prog_unknow;
		else if (_ver != key.vers)
			result.second = ver_mismatch;

		_enter_read([&]()
		{
			result.first = _stubs.find(key.proc);
			if (result.first == _stubs.end())
			{
				result.second = proc_mismatch;
			}
			else
			{
				result.second = find_success;
			}
		});
		return result;
	}

	void erase(const const_iterator& where)
	{
		_enter_write([&]()
		{
			_stubs.erase(where);
		});
	}

	void unregister(const ident_type& key)
	{
		if (key.prog != _prog || key.ver != _ver)
			return;
		_enter_write([&]()
		{
			_stubs.erase(key.proc);
		});
	}

	register_result registe(const value_type& value)
	{
		register_result result;
		_enter_write([&]()
		{
			result = emplace_register(value.first, value.second);
		});
		return result;
	}

	template<class... Arg>
	register_result emplace_register(const stub_ident& ident,Arg&&... args)
	{
		register_result result;
		if (ident.prog != _prog || ident.vers != _ver)
			return std::make_pair(_stubs.end(), false);
		_enter_write([&]()
		{
			result = _stubs.emplace(ident.proc, args...);
		});
		return result;
	}

	size_type size() const throw()
	{
		size_type result = 0;
		_enter_read([&]()
		{
			result = _stubs.size();
		});
		return result;
	}

private:
	template<class Func>
	void _enter_write(Func fn)
	{
		_spinlock.lock();
		fn();
		_spinlock.unlock();
	}
	
	template<class Func>
	void _enter_read(Func fn) const
	{
		auto& lock=const_cast<self_type*>(this)->_spinlock;
		lock.lock();
		fn();
		lock.unlock();
	}

	typedef boost::detail::spinlock spinlock_t;

	size_type _prog, _ver;

	spinlock_t _spinlock;
	impl _stubs;
};

//用于分配ident_allocer
class stub_ident_allocer
{
public:
	typedef xdr_uint32 size_type;
	typedef xdr_uint32::source_type local_size_type;
	typedef std::set<stub_ident> impl;
	typedef impl::iterator iterator;
	typedef impl::const_iterator const_iterator;
	/*
		程序号,程序号的默认版本
	*/
	stub_ident_allocer(const size_type& progid, const size_type& default_ver);

	typedef std::pair<iterator, bool> alloc_result;
	/*
		分配一个id
	*/
	alloc_result try_alloc(const size_type& procid);
	alloc_result try_alloc(const size_type& procid, const size_type& ver);

	/*
		当前有多少个proc
	*/
	local_size_type size() const;

	/*
		删除一个过程,版本号为default_ver
	*/
	void erase(const size_type& procid);

	/*
		删除一个过程并且指定版本号
	*/
	void erase(const size_type& procid, const size_type& ver);

	/*
		获取该ident
	*/
	const_iterator ident(const size_type& procid, const size_type& ver) const;

	/*
		获取ident,但是版本号用默认的
	*/
	const_iterator ident(const size_type& procid) const;

	const_iterator begin() const throw();
	const_iterator end() const throw();

	bool empty() const throw();
private:
	size_type _progid;
	size_type _ver;
	impl _alloced;
};

typedef serialize <
	serialize_object <
	condition_filter <
	is_rarchiveable, xdr_filter > > > server_default_serialzie;

template<class IdentType, class R, class... Arg>
auto make_server_stub(const IdentType& ident, R(*fn)(Arg...))
->decltype(basic_make_server_stub<server_default_serialzie>(ident, fn))
{
	return basic_make_server_stub<server_default_serialzie>(ident, fn);
}

class stub_register_failed
	:public std::runtime_error
{
public:
	stub_register_failed(const stub_ident& ident)
		:std::runtime_error("register server stub faild")
	{}
};

/*
	生成server_stub,但不绑定到具名变量上
	*/
#define MAKE_SERVER_STUB(prog,proc,ver,func)\
	make_server_stub(stub_ident(prog,ver,proc),func)

/*
	生成server_stub,并放到堆中
	*/
#define GENERIC_SERVER_STUB_PTR(prog,proc,ver,func)\
	new real_type<decltype(MAKE_SERVER_STUB(prog,proc,ver,func))>::type\
		(MAKE_SERVER_STUB(prog,proc,ver,func))

/*
	生成server_stub并绑定到变量
	*/
#define GENERIC_GLOBAL_SPACE_STUB(stub_name,prog,proc,ver,func)\
	auto stub_name=MAKE_SERVER_STUB(prog,proc,ver,func)

/*
	生成server_stub,并绑定到shared_ptr
	*/
#define GENERIC_SERVER_STUB_CODE_IN_HEAP(prog,proc,ver,func)\
	std::make_shared(GENERIC_SERVER_STUB_PTR(prog,proc,ver,func))

/*
	生成server_stub,绑定到shared_ptr,最后绑定到变量
	*/
#define  GENERIC_GLOBACL_SPACE_HEAP_SERVER_STUB(stub_name,prog,proc,ver,func)\
	auto stub_name=GENERIC_SERVER_STUB_CODE_IN_HEAP(prog,proc,ver,func)

#define REGISTER_SERVER_STUB(pool,prog,proc,ver,ptr)\
		stub_ident ident(prog,ver,proc);\
		auto pair=pool.emplace_register(ident,ptr);\
		if(!pair.second)\
			throw stub_register_failed(ident);\
/*
	生成server_stub,注册到stub pool,绑定到变量,注册失败时
	出错时会产生异常
	*/
#define GENERIC_GLOBAL_SERVER_STUB_AND_REGISTER(pool,stub_name,prog,proc,ver,func)\
	auto stub_name=(\
	[&]()->decltype(MAKE_SERVER_STUB(prog,proc,ver,func))\
	{\
		auto* ptr=GENERIC_SERVER_STUB_PTR(prog,proc,ver,func);\
		REGISTER_SERVER_STUB(pool,prog,proc,ver,ptr)\
		return *ptr;\
	}())

/*
	^,只是放到heap
	*/
#define GENERIC_HEAP_SERVER_STUB_AND_REGISTER(pool,stub_name,prog,proc,ver,func)\
	auto* ptr=GENERIC_SERVER_STUB_PTR(prog,proc,ver,func);\
	auto* ptr2=GENERIC_SERVER_STUB_PTR(prog,proc,ver,func);\
	REGISTER_SERVER_STUB(prog,proc,ver,ptr)\
	auto stub_name=std::make_shared(ptr2)

/*
	用于静态绑定ID,并提供重复ID的编译错误
*/
template<std::size_t ProgId,std::size_t ProcId>
struct stub_ident_holder
{
	static_assert(ProgId >= 0x40000000, "rpc progid must be ge than 0x40000000");
};

#define STATIC_ALLOC_STUB_IDENT(prog,proc)\
	template<> struct stub_ident_holder<prog,proc>{\
	static_assert(prog >= 0x40000000, "rpc progid must be ge than 0x40000000");\
	};
/*
	生成服务器桩,但是假设proc或者prog号码被占用则会发生编译错误
	流程-静态确认桩id是否被分配,注册到pool并生成全局对象
*/
#define GENERIC_SERVER_STUB(pool,stub_name,prog,proc,ver,func)\
	STATIC_ALLOC_STUB_IDENT(prog,proc)\
	GENERIC_GLOBAL_SERVER_STUB_AND_REGISTER(pool,stub_name,\
	prog,proc,ver,func)

#define GENERIC_SERVER_STUB_IN_HEAD(pool,stub_name,prog,proc,ver,func)\
	STATIC_ALLOC_STUB_IDENT(prog,proc)\
	GENERIC_HEAP_SERVER_STUB_AND_REGISTER(pool,stub_name,\
	prog,proc,ver,func)

#endif