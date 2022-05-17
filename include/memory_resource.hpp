// <memory_resource> -*- C++ -*-

// Copyright (C) 2018-2022 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file include/memory_resource
 *  This is a Standard C++ Library header.
 */

#ifndef _GLIBCXX_MEMORY_RESOURCE
#define _GLIBCXX_MEMORY_RESOURCE 1

#pragma GCC system_header

#if __cplusplus >= 201703L

#include <cassert>
#include <cstddef>			// size_t, max_align_t, byte
#include <limits>
#include <memory>			// align
#include <new>
#include <shared_mutex>			// shared_mutex
#include <vector>			// vector
#include <threads.h>

#if ! __cpp_lib_make_obj_using_allocator
# include <utility> // index_sequence
# include <tuple>   // tuple, forward_as_tuple
#endif

#if __cplusplus > 201703L
#define _GLIBCXX20_CONSTEXPR constexpr
#define _GLIBCXX20_DEPRECATED_SUGGEST(m) [[deprecated("Superseded by " m)]]
#else
#define _GLIBCXX20_CONSTEXPR
#define _GLIBCXX20_DEPRECATED_SUGGEST(m)
#endif

namespace std
{
  namespace pmr
  {
  // Header and all contents are present.
# define __cpp_lib_memory_resource 201603L

  class memory_resource;

#if __cplusplus == 201703L
  template<typename _Tp>
  class polymorphic_allocator;
#else // C++20
# define __cpp_lib_polymorphic_allocator 201902L
  template<typename _Tp = std::byte>
  class polymorphic_allocator;
#endif

  // Global memory resources
  memory_resource* new_delete_resource() noexcept;
  memory_resource* null_memory_resource() noexcept;
  memory_resource* set_default_resource(memory_resource* __r) noexcept;
  memory_resource* get_default_resource() noexcept
      __attribute__((__returns_nonnull__));

  // Pool resource classes
  struct pool_options;
  class synchronized_pool_resource;
  class unsynchronized_pool_resource;
  class monotonic_buffer_resource;

  // pmr vector
  template <class T>
  using vector = std::vector<T, std::pmr::polymorphic_allocator<T>>;

  /// Class memory_resource
  class memory_resource
  {
    static constexpr size_t _S_max_align = alignof(max_align_t);

  public:
    memory_resource() = default;
    memory_resource(const memory_resource&) = default;
    virtual ~memory_resource(); // key function

    memory_resource& operator=(const memory_resource&) = default;

    [[nodiscard]]
    void*
    allocate(size_t __bytes, size_t __alignment = _S_max_align)
        __attribute__((__returns_nonnull__,__alloc_size__(2),__alloc_align__(3)))
    { return ::operator new(__bytes, do_allocate(__bytes, __alignment)); }

    void
    deallocate(void* __p, size_t __bytes, size_t __alignment = _S_max_align)
        __attribute__((__nonnull__))
    { return do_deallocate(__p, __bytes, __alignment); }

    bool
    is_equal(const memory_resource& __other) const noexcept
    { return do_is_equal(__other); }

  private:
    virtual void*
    do_allocate(size_t __bytes, size_t __alignment) = 0;

    virtual void
    do_deallocate(void* __p, size_t __bytes, size_t __alignment) = 0;

    virtual bool
    do_is_equal(const memory_resource& __other) const noexcept = 0;
  };

  inline bool
  operator==(const memory_resource& __a, const memory_resource& __b) noexcept
  { return &__a == &__b || __a.is_equal(__b); }

#if __cpp_impl_three_way_comparison < 201907L
  inline bool
  operator!=(const memory_resource& __a, const memory_resource& __b) noexcept
  { return !(__a == __b); }
#endif

  // C++17 23.12.3 Class template polymorphic_allocator
  template<typename _Tp>
  class polymorphic_allocator
  {
    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 2975. Missing case for pair construction in polymorphic allocators
    template<typename _Up>
    struct __not_pair { using type = void; };

    template<typename _Up1, typename _Up2>
    struct __not_pair<pair<_Up1, _Up2>> { };

  public:
    using value_type = _Tp;

    polymorphic_allocator() noexcept
        : _M_resource(get_default_resource())
    { }

    polymorphic_allocator(memory_resource* __r) noexcept
        __attribute__((__nonnull__))
        : _M_resource(__r)
    { assert(__r); }

    polymorphic_allocator(const polymorphic_allocator& __other) = default;

    template<typename _Up>
    polymorphic_allocator(const polymorphic_allocator<_Up>& __x) noexcept
        : _M_resource(__x.resource())
    { }

    polymorphic_allocator&
    operator=(const polymorphic_allocator&) = delete;

    [[nodiscard]]
    _Tp*
    allocate(size_t __n)
        __attribute__((__returns_nonnull__))
    {
      if ((std::numeric_limits<size_t>::max() / sizeof(_Tp)) < __n)
        std::__throw_bad_array_new_length();
      return static_cast<_Tp*>(_M_resource->allocate(__n * sizeof(_Tp),
                                                      alignof(_Tp)));
    }

    void
    deallocate(_Tp* __p, size_t __n) noexcept
        __attribute__((__nonnull__))
    { _M_resource->deallocate(__p, __n * sizeof(_Tp), alignof(_Tp)); }

#if __cplusplus > 201703L
    [[nodiscard]] void*
    allocate_bytes(size_t __nbytes,
                   size_t __alignment = alignof(max_align_t))
    { return _M_resource->allocate(__nbytes, __alignment); }

    void
    deallocate_bytes(void* __p, size_t __nbytes,
                     size_t __alignment = alignof(max_align_t))
    { _M_resource->deallocate(__p, __nbytes, __alignment); }

    template<typename _Up>
    [[nodiscard]] _Up*
    allocate_object(size_t __n = 1)
    {
      if ((std::numeric_limits<size_t>::max() / sizeof(_Up)) < __n)
        std::__throw_bad_array_new_length();
      return static_cast<_Up*>(allocate_bytes(__n * sizeof(_Up),
                                               alignof(_Up)));
    }

    template<typename _Up>
    void
    deallocate_object(_Up* __p, size_t __n = 1)
    { deallocate_bytes(__p, __n * sizeof(_Up), alignof(_Up)); }

    template<typename _Up, typename... _CtorArgs>
    [[nodiscard]] _Up*
    new_object(_CtorArgs&&... __ctor_args)
    {
      _Up* __p = allocate_object<_Up>();
      try
      {
        construct(__p, std::forward<_CtorArgs>(__ctor_args)...);
      }
      catch (...)
      {
        deallocate_object(__p);
        throw;
      }
      return __p;
    }

    template<typename _Up>
    void
    delete_object(_Up* __p)
    {
      __p->~_Up();
      deallocate_object(__p);
    }
#endif // C++2a

#if ! __cpp_lib_make_obj_using_allocator
    template<typename _Tp1, typename... _Args>
    __attribute__((__nonnull__))
    typename __not_pair<_Tp1>::type
    construct(_Tp1* __p, _Args&&... __args)
    {
      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 2969. polymorphic_allocator::construct() shouldn't pass resource()
#ifdef _LIBCPP_VERSION
      __user_alloc_construct_impl(
          __uses_alloc_ctor<_Tp1, polymorphic_allocator<_Tp1>, _Args...>{},
          __p, *this, std::forward<_Args>(__args)...);
#else
      using __use_tag
          = std::__uses_alloc_t<_Tp1, polymorphic_allocator, _Args...>;
      if constexpr (is_base_of_v<__uses_alloc0, __use_tag>)
        ::new(__p) _Tp1(std::forward<_Args>(__args)...);
      else if constexpr (is_base_of_v<__uses_alloc1_, __use_tag>)
        ::new(__p) _Tp1(allocator_arg, *this,
                         std::forward<_Args>(__args)...);
      else
        ::new(__p) _Tp1(std::forward<_Args>(__args)..., *this);
#endif
    }

    template<typename _Tp1, typename _Tp2,
              typename... _Args1, typename... _Args2>
    __attribute__((__nonnull__))
    void
    construct(pair<_Tp1, _Tp2>* __p, piecewise_construct_t,
              tuple<_Args1...> __x, tuple<_Args2...> __y)
    {
      auto __x_tag =
          __use_alloc<_Tp1, polymorphic_allocator, _Args1...>(*this);
      auto __y_tag =
          __use_alloc<_Tp2, polymorphic_allocator, _Args2...>(*this);
      index_sequence_for<_Args1...> __x_i;
      index_sequence_for<_Args2...> __y_i;

      ::new(__p) pair<_Tp1, _Tp2>(piecewise_construct,
                                   _S_construct_p(__x_tag, __x_i, __x, *this),
                                   _S_construct_p(__y_tag, __y_i, __y, *this));
    }

    template<typename _Tp1, typename _Tp2>
    __attribute__((__nonnull__))
    void
    construct(pair<_Tp1, _Tp2>* __p)
    { this->construct(__p, piecewise_construct, tuple<>(), tuple<>()); }

    template<typename _Tp1, typename _Tp2, typename _Up, typename _Vp>
    __attribute__((__nonnull__))
    void
    construct(pair<_Tp1, _Tp2>* __p, _Up&& __x, _Vp&& __y)
    {
      this->construct(__p, piecewise_construct,
                      std::forward_as_tuple(std::forward<_Up>(__x)),
                      std::forward_as_tuple(std::forward<_Vp>(__y)));
    }

    template <typename _Tp1, typename _Tp2, typename _Up, typename _Vp>
    __attribute__((__nonnull__))
    void
    construct(pair<_Tp1, _Tp2>* __p, const std::pair<_Up, _Vp>& __pr)
    {
      this->construct(__p, piecewise_construct,
                      std::forward_as_tuple(__pr.first),
                      std::forward_as_tuple(__pr.second));
    }

    template<typename _Tp1, typename _Tp2, typename _Up, typename _Vp>
    __attribute__((__nonnull__))
    void
    construct(pair<_Tp1, _Tp2>* __p, pair<_Up, _Vp>&& __pr)
    {
      this->construct(__p, piecewise_construct,
                      std::forward_as_tuple(std::forward<_Up>(__pr.first)),
                      std::forward_as_tuple(std::forward<_Vp>(__pr.second)));
    }
#else // make_obj_using_allocator
    template<typename _Tp1, typename... _Args>
    __attribute__((__nonnull__))
    void
    construct(_Tp1* __p, _Args&&... __args)
    {
      std::uninitialized_construct_using_allocator(__p, *this,
                                                   std::forward<_Args>(__args)...);
    }
#endif

    template<typename _Up>
    _GLIBCXX20_DEPRECATED_SUGGEST("allocator_traits::destroy")
    __attribute__((__nonnull__))
    void
        destroy(_Up* __p)
    { __p->~_Up(); }

    polymorphic_allocator
    select_on_container_copy_construction() const noexcept
    { return polymorphic_allocator(); }

    memory_resource*
    resource() const noexcept
        __attribute__((__returns_nonnull__))
    { return _M_resource; }

  private:
#if ! __cpp_lib_make_obj_using_allocator
#ifdef _LIBCPP_VERSION
    template<typename _Ind, typename... _Args>
    static tuple<_Args&&...>
    _S_construct_p(integral_constant<int, 0>, _Ind, tuple<_Args...>& __t,
                   polymorphic_allocator&)
    { return std::move(__t); }

    template<size_t... _Ind, typename... _Args>
    static tuple<allocator_arg_t, polymorphic_allocator, _Args&&...>
    _S_construct_p(integral_constant<int, 1>, index_sequence<_Ind...>,
                   tuple<_Args...>& __t, polymorphic_allocator& __ua)
    {
      return {
          allocator_arg, *__ua._M_a, std::get<_Ind>(std::move(__t))...
      };
    }

    template<size_t... _Ind, typename... _Args>
    static tuple<_Args&&..., polymorphic_allocator>
    _S_construct_p(integral_constant<int, 2>, index_sequence<_Ind...>,
                   tuple<_Args...>& __t, polymorphic_allocator& __ua)
    { return { std::get<_Ind>(std::move(__t))..., *__ua._M_a }; }
#else

    using __uses_alloc1_ = __uses_alloc1<polymorphic_allocator>;
    using __uses_alloc2_ = __uses_alloc2<polymorphic_allocator>;

    template<typename _Ind, typename... _Args>
    static tuple<_Args&&...>
    _S_construct_p(__uses_alloc0, _Ind, tuple<_Args...>& __t)
    { return std::move(__t); }

    template<size_t... _Ind, typename... _Args>
    static tuple<allocator_arg_t, polymorphic_allocator, _Args&&...>
    _S_construct_p(__uses_alloc1_ __ua, index_sequence<_Ind...>,
                   tuple<_Args...>& __t)
    {
      return {
          allocator_arg, *__ua._M_a, std::get<_Ind>(std::move(__t))...
      };
    }

    template<size_t... _Ind, typename... _Args>
    static tuple<_Args&&..., polymorphic_allocator>
    _S_construct_p(__uses_alloc2_ __ua, index_sequence<_Ind...>,
                   tuple<_Args...>& __t)
    { return { std::get<_Ind>(std::move(__t))..., *__ua._M_a }; }
#endif
#endif

    memory_resource* _M_resource;
  };

  template<typename _Tp1, typename _Tp2>
  inline bool
  operator==(const polymorphic_allocator<_Tp1>& __a,
             const polymorphic_allocator<_Tp2>& __b) noexcept
  { return *__a.resource() == *__b.resource(); }

#if __cpp_impl_three_way_comparison < 201907L
  template<typename _Tp1, typename _Tp2>
  inline bool
  operator!=(const polymorphic_allocator<_Tp1>& __a,
             const polymorphic_allocator<_Tp2>& __b) noexcept
  { return !(__a == __b); }
#endif

  } // namespace pmr

  /// Partial specialization for std::pmr::polymorphic_allocator
  template<typename _Tp>
  struct allocator_traits<pmr::polymorphic_allocator<_Tp>>
  {
    /// The allocator type
    using allocator_type = pmr::polymorphic_allocator<_Tp>;

    /// The allocated type
    using value_type = _Tp;

    /// The allocator's pointer type.
    using pointer = _Tp*;

    /// The allocator's const pointer type.
    using const_pointer = const _Tp*;

    /// The allocator's void pointer type.
    using void_pointer = void*;

    /// The allocator's const void pointer type.
    using const_void_pointer = const void*;

    /// The allocator's difference type
    using difference_type = std::ptrdiff_t;

    /// The allocator's size type
    using size_type = std::size_t;

    /** @{
       * A `polymorphic_allocator` does not propagate when a
       * container is copied, moved, or swapped.
     */
    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_move_assignment = false_type;
    using propagate_on_container_swap = false_type;

    static allocator_type
    select_on_container_copy_construction(const allocator_type&) noexcept
    { return allocator_type(); }
    /// @}

    /// Whether all instances of the allocator type compare equal.
    using is_always_equal = false_type;

    template<typename _Up>
    using rebind_alloc = pmr::polymorphic_allocator<_Up>;

    template<typename _Up>
    using rebind_traits = allocator_traits<pmr::polymorphic_allocator<_Up>>;

    /**
       *  @brief  Allocate memory.
       *  @param  __a  An allocator.
       *  @param  __n  The number of objects to allocate space for.
       *
       *  Calls `a.allocate(n)`.
     */
    [[nodiscard]] static pointer
    allocate(allocator_type& __a, size_type __n)
    { return __a.allocate(__n); }

    /**
       *  @brief  Allocate memory.
       *  @param  __a  An allocator.
       *  @param  __n  The number of objects to allocate space for.
       *  @return Memory of suitable size and alignment for `n` objects
       *          of type `value_type`.
       *
       *  The third parameter is ignored..
       *
       *  Returns `a.allocate(n)`.
     */
    [[nodiscard]] static pointer
    allocate(allocator_type& __a, size_type __n, const_void_pointer)
    { return __a.allocate(__n); }

    /**
       *  @brief  Deallocate memory.
       *  @param  __a  An allocator.
       *  @param  __p  Pointer to the memory to deallocate.
       *  @param  __n  The number of objects space was allocated for.
       *
       *  Calls `a.deallocate(p, n)`.
     */
    static void
    deallocate(allocator_type& __a, pointer __p, size_type __n)
    { __a.deallocate(__p, __n); }

    /**
       *  @brief  Construct an object of type `_Up`
       *  @param  __a  An allocator.
       *  @param  __p  Pointer to memory of suitable size and alignment for
       *	       an object of type `_Up`.
       *  @param  __args Constructor arguments.
       *
       *  Calls `__a.construct(__p, std::forward<_Args>(__args)...)`
       *  in C++11, C++14 and C++17. Changed in C++20 to call
       *  `std::construct_at(__p, std::forward<_Args>(__args)...)` instead.
     */
    template<typename _Up, typename... _Args>
    static void
    construct(allocator_type& __a, _Up* __p, _Args&&... __args)
    { __a.construct(__p, std::forward<_Args>(__args)...); }

    /**
       *  @brief  Destroy an object of type `_Up`
       *  @param  __a  An allocator.
       *  @param  __p  Pointer to the object to destroy
       *
       *  Calls `p->_Up()`.
     */
    template<typename _Up>
    static _GLIBCXX20_CONSTEXPR void
    destroy(allocator_type&, _Up* __p)
        noexcept(is_nothrow_destructible<_Up>::value)
    { __p->~_Up(); }

    /**
       *  @brief  The maximum supported allocation size
       *  @return `numeric_limits<size_t>::max() / sizeof(value_type)`
     */
    static _GLIBCXX20_CONSTEXPR size_type
    max_size(const allocator_type&) noexcept
    { return size_t(-1) / sizeof(value_type); }
  };

  namespace pmr
  {
  /// Parameters for tuning a pool resource's behaviour.
  struct pool_options
  {
    /** @brief Upper limit on number of blocks in a chunk.
     *
     * A lower value prevents allocating huge chunks that could remain mostly
     * unused, but means pools will need to replenished more frequently.
     */
    size_t max_blocks_per_chunk = 0;

    /* @brief Largest block size (in bytes) that should be served from pools.
     *
     * Larger allocations will be served directly by the upstream resource,
     * not from one of the pools managed by the pool resource.
     */
    size_t largest_required_pool_block = 0;
  };

  // Common implementation details for un-/synchronized pool resources.
  class __pool_resource
  {
    friend class synchronized_pool_resource;
    friend class unsynchronized_pool_resource;

    __pool_resource(const pool_options& __opts, memory_resource* __upstream);

    ~__pool_resource();

    __pool_resource(const __pool_resource&) = delete;
    __pool_resource& operator=(const __pool_resource&) = delete;

    // Allocate a large unpooled block.
    void*
    allocate(size_t __bytes, size_t __alignment);

    // Deallocate a large unpooled block.
    void
    deallocate(void* __p, size_t __bytes, size_t __alignment);


    // Deallocate unpooled memory.
    void release() noexcept;

    memory_resource* resource() const noexcept
    { return _M_unpooled.get_allocator().resource(); }

    struct _Pool;

    _Pool* _M_alloc_pools();

    const pool_options _M_opts;

    struct _BigBlock;
    // Collection of blocks too big for any pool, sorted by address.
    // This also stores the only copy of the upstream memory resource pointer.
    std::pmr::vector<_BigBlock> _M_unpooled;

    const int _M_npools;
  };

  /// A thread-safe memory resource that manages pools of fixed-size blocks.
  class synchronized_pool_resource : public memory_resource
  {
  public:
    synchronized_pool_resource(const pool_options& __opts,
                               memory_resource* __upstream)
        __attribute__((__nonnull__));

    synchronized_pool_resource()
        : synchronized_pool_resource(pool_options(), get_default_resource())
    { }

    explicit
        synchronized_pool_resource(memory_resource* __upstream)
            __attribute__((__nonnull__))
            : synchronized_pool_resource(pool_options(), __upstream)
    { }

    explicit
        synchronized_pool_resource(const pool_options& __opts)
        : synchronized_pool_resource(__opts, get_default_resource()) { }

    synchronized_pool_resource(const synchronized_pool_resource&) = delete;

    virtual ~synchronized_pool_resource();

    synchronized_pool_resource&
    operator=(const synchronized_pool_resource&) = delete;

    void release();

    memory_resource*
    upstream_resource() const noexcept
        __attribute__((__returns_nonnull__))
    { return _M_impl.resource(); }

    pool_options options() const noexcept { return _M_impl._M_opts; }

  protected:
    void*
    do_allocate(size_t __bytes, size_t __alignment) override;

    void
    do_deallocate(void* __p, size_t __bytes, size_t __alignment) override;

    bool
    do_is_equal(const memory_resource& __other) const noexcept override
    { return this == &__other; }

  public:
    // Thread-specific pools (only public for access by implementation details)
    struct _TPools;

  private:
    _TPools* _M_alloc_tpools(lock_guard<shared_mutex>&);
    _TPools* _M_alloc_shared_tpools(lock_guard<shared_mutex>&);
    auto _M_thread_specific_pools() noexcept;

    __pool_resource _M_impl;
    ::tss_t _M_key;
    // Linked list of thread-specific pools. All threads share _M_tpools[0].
    _TPools* _M_tpools = nullptr;
    mutable shared_mutex _M_mx;
  };
#endif

  /// A non-thread-safe memory resource that manages pools of fixed-size blocks.
  class unsynchronized_pool_resource : public memory_resource
  {
  public:
    [[__gnu__::__nonnull__]]
    unsynchronized_pool_resource(const pool_options& __opts,
                                 memory_resource* __upstream);

    unsynchronized_pool_resource()
        : unsynchronized_pool_resource(pool_options(), get_default_resource())
    { }

    [[__gnu__::__nonnull__]]
    explicit
        unsynchronized_pool_resource(memory_resource* __upstream)
        : unsynchronized_pool_resource(pool_options(), __upstream)
    { }

    explicit
        unsynchronized_pool_resource(const pool_options& __opts)
        : unsynchronized_pool_resource(__opts, get_default_resource()) { }

    unsynchronized_pool_resource(const unsynchronized_pool_resource&) = delete;

    virtual ~unsynchronized_pool_resource();

    unsynchronized_pool_resource&
    operator=(const unsynchronized_pool_resource&) = delete;

    void release();

    [[__gnu__::__returns_nonnull__]]
    memory_resource*
    upstream_resource() const noexcept
    { return _M_impl.resource(); }

    pool_options options() const noexcept { return _M_impl._M_opts; }

  protected:
    void*
    do_allocate(size_t __bytes, size_t __alignment) override;

    void
    do_deallocate(void* __p, size_t __bytes, size_t __alignment) override;

    bool
    do_is_equal(const memory_resource& __other) const noexcept override
    { return this == &__other; }

  private:
    using _Pool = __pool_resource::_Pool;

    auto _M_find_pool(size_t) noexcept;

    __pool_resource _M_impl;
    _Pool* _M_pools = nullptr;
  };

  class monotonic_buffer_resource : public memory_resource
  {
  public:
    explicit
        monotonic_buffer_resource(memory_resource* __upstream) noexcept
        __attribute__((__nonnull__))
        : _M_upstream(__upstream)
    { assert(__upstream != nullptr); }

    monotonic_buffer_resource(size_t __initial_size,
                              memory_resource* __upstream) noexcept
        __attribute__((__nonnull__))
        : _M_next_bufsiz(__initial_size),
          _M_upstream(__upstream)
    {
      assert(__upstream != nullptr);
      assert(__initial_size > 0);
    }

    monotonic_buffer_resource(void* __buffer, size_t __buffer_size,
                              memory_resource* __upstream) noexcept
        __attribute__((__nonnull__(4)))
        : _M_current_buf(__buffer), _M_avail(__buffer_size),
          _M_next_bufsiz(_S_next_bufsize(__buffer_size)),
          _M_upstream(__upstream),
          _M_orig_buf(__buffer), _M_orig_size(__buffer_size)
    {
      assert(__upstream != nullptr);
      assert(__buffer != nullptr || __buffer_size == 0);
    }

    monotonic_buffer_resource() noexcept
        : monotonic_buffer_resource(get_default_resource())
    { }

    explicit
        monotonic_buffer_resource(size_t __initial_size) noexcept
        : monotonic_buffer_resource(__initial_size, get_default_resource())
    { }

    monotonic_buffer_resource(void* __buffer, size_t __buffer_size) noexcept
        : monotonic_buffer_resource(__buffer, __buffer_size, get_default_resource())
    { }

    monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;

    virtual ~monotonic_buffer_resource(); // key function

    monotonic_buffer_resource&
    operator=(const monotonic_buffer_resource&) = delete;

    void
    release() noexcept
    {
      if (_M_head)
        _M_release_buffers();

      // reset to initial state at contruction:
      if ((_M_current_buf = _M_orig_buf))
      {
        _M_avail = _M_orig_size;
        _M_next_bufsiz = _S_next_bufsize(_M_orig_size);
      }
      else
      {
        _M_avail = 0;
        _M_next_bufsiz = _M_orig_size;
      }
    }

    memory_resource*
    upstream_resource() const noexcept
        __attribute__((__returns_nonnull__))
    { return _M_upstream; }

  protected:
    void*
    do_allocate(size_t __bytes, size_t __alignment) override
    {
      if (__builtin_expect(__bytes == 0, false))
        __bytes = 1; // Ensures we don't return the same pointer twice.

      void* __p = std::align(__alignment, __bytes, _M_current_buf, _M_avail);
      if (__builtin_expect(__p == nullptr, false))
      {
        _M_new_buffer(__bytes, __alignment);
        __p = _M_current_buf;
      }
      _M_current_buf = (char*)_M_current_buf + __bytes;
      _M_avail -= __bytes;
      return __p;
    }

    void
    do_deallocate(void*, size_t, size_t) override
    { }

    bool
    do_is_equal(const memory_resource& __other) const noexcept override
    { return this == &__other; }

  private:
    // Update _M_current_buf and _M_avail to refer to a new buffer with
    // at least the specified size and alignment, allocated from upstream.
    void
    _M_new_buffer(size_t __bytes, size_t __alignment);

    // Deallocate all buffers obtained from upstream.
    void
    _M_release_buffers() noexcept;

    static size_t
    _S_next_bufsize(size_t __buffer_size) noexcept
    {
      if (__builtin_expect(__buffer_size == 0, false))
        __buffer_size = 1;
      return __buffer_size * _S_growth_factor;
    }

    static constexpr size_t _S_init_bufsize = 128 * sizeof(void*);
    static constexpr float _S_growth_factor = 1.5;

    void*	_M_current_buf = nullptr;
    size_t	_M_avail = 0;
    size_t	_M_next_bufsiz = _S_init_bufsize;

    // Initial values set at construction and reused by release():
    memory_resource* const	_M_upstream;
    void* const			_M_orig_buf = nullptr;
    size_t const		_M_orig_size = _M_next_bufsiz;

    class _Chunk;
    _Chunk* _M_head = nullptr;
  };

  } // namespace pmr
} // namespace std

#endif // _GLIBCXX_MEMORY_RESOURCE
