//  Copyright (c) 2007-2014 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_PARALLEL_UTIL_LOOP_MAY_27_2014_1040PM)
#define HPX_PARALLEL_UTIL_LOOP_MAY_27_2014_1040PM

#include <hpx/hpx_fwd.hpp>
#include <hpx/parallel/util/cancellation_token.hpp>

#include <iterator>
#include <algorithm>
#include <boost/range/irange.hpp>

namespace hpx { namespace parallel { namespace util
{
    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        ///////////////////////////////////////////////////////////////////////
        // Helper class to repeatedly call a function starting from a given
        // iterator position.
        template <typename IterCat>
        struct loop
        {
            ///////////////////////////////////////////////////////////////////
            template <typename Begin, typename End, typename F>
            static Begin call(Begin it, End end, F && f)
            {
                for (/**/; it != end; ++it)
                    f(it);

                return it;
            }

            template <typename Begin, typename End, typename CancelToken,
                typename F>
            static Begin call(Begin it, End end, CancelToken& tok, F && func)
            {
                for (/**/; it != end; ++it)
                {
                    if (tok.was_cancelled())
                        break;
                    func(it);
                }
                return it;
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename Begin, typename End, typename F>
    HPX_FORCEINLINE Begin
    loop(Begin begin, End end, F && f)
    {
        typedef typename std::iterator_traits<Begin>::iterator_category cat;
        return detail::loop<cat>::call(begin, end, std::forward<F>(f));
    }

    template <typename Begin, typename End, typename CancelToken, typename F>
    HPX_FORCEINLINE Begin
    loop(Begin begin, End end, CancelToken& tok, F && f)
    {
        typedef typename std::iterator_traits<Begin>::iterator_category cat;
        return detail::loop<cat>::call(begin, end, tok, std::forward<F>(f));
    };

    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {

        //New random access iterator which is used for prefetching containers within lambda functions
        template<typename T>
        class prefetching_iterator: public std::iterator<std::random_access_iterator_tag, std::size_t>
        {
            public:

            using base_iterator = std::vector<std::size_t>::iterator;

            std::vector< T * > M_;
            base_iterator base;
            std::size_t chunk_size;
            std::size_t range_size;
            std::size_t idx;

            explicit prefetching_iterator(std::size_t idx_,base_iterator base_ , std::size_t chunk_size_,
                std::size_t range_size_, std::vector< T * > const & A)
            : M_(A), base(base_), chunk_size(chunk_size_), range_size(range_size_), idx(idx_) {}

            using difference_type = typename std::iterator<std::random_access_iterator_tag, std::size_t>::difference_type;

            inline prefetching_iterator& operator+=(difference_type rhs)
            {
                idx = idx + (rhs*chunk_size);
                base = base + (rhs*chunk_size);
                return *this;
            }

            inline prefetching_iterator& operator-=(difference_type rhs)
            {
                idx = idx - (rhs*chunk_size);
                base = base - (rhs*chunk_size);
                return *this;
            }

            inline prefetching_iterator& operator++()
            {
                idx = idx + chunk_size;
                base = base + chunk_size;
                return *this;
            }

            inline prefetching_iterator& operator--()
            {
                idx = idx - chunk_size;
                base = base - chunk_size;
                return *this;
            }

            inline prefetching_iterator operator++(int)
            {
                prefetching_iterator tmp(*this);
                operator++();
                return tmp;
            }

            inline prefetching_iterator operator--(int)
            {
                prefetching_iterator tmp(*this);
                operator--();
                return tmp;
            }

            inline difference_type operator-(const prefetching_iterator& rhs) const
            {
                return (idx-rhs.idx)/chunk_size;
            }

            inline prefetching_iterator operator+(difference_type rhs) const
            {
                return prefetching_iterator((idx+(rhs*chunk_size)),(base+(rhs*chunk_size)),chunk_size,range_size,M_);
            }

            inline prefetching_iterator operator-(difference_type rhs) const
            {
                return prefetching_iterator((idx-(rhs*chunk_size)),(base-(rhs*chunk_size)),chunk_size,range_size,M_);
            }

            friend inline prefetching_iterator operator+(difference_type lhs, const prefetching_iterator& rhs)
            {
                return rhs + lhs;
            }

            friend inline prefetching_iterator operator-(difference_type lhs, const prefetching_iterator& rhs)
            {
                return lhs - rhs;
            }


            inline bool operator==(const prefetching_iterator& rhs) const {return idx == rhs.idx;}
            inline bool operator!=(const prefetching_iterator& rhs) const {return idx != rhs.idx;}
            inline bool operator>(const prefetching_iterator& rhs) const {return idx > rhs.idx;}
            inline bool operator<(const prefetching_iterator& rhs) const {return idx < rhs.idx;}
            inline bool operator>=(const prefetching_iterator& rhs) const {return idx >= rhs.idx;}
            inline bool operator<=(const prefetching_iterator& rhs) const {return idx <= rhs.idx;}


            inline std::size_t operator*() const {return idx;}
        };


        //Helper class to initialize prefetching_iterator
        template<typename T>
        struct prefetcher_context
        {
            std::vector<std::size_t> range;
            std::vector<std::size_t>::iterator it_begin;
            std::vector<std::size_t>::iterator it_end;
            std::size_t prefetcher_distance_factor;
            std::size_t chunk_size;
            std::vector< T * > m;
            std::size_t range_size;


            explicit prefetcher_context (std::size_t begin, std::size_t end,
                std::size_t p_factor, std::initializer_list< T * > &&l)
            {
                std::size_t vector_size = end - begin;
                range.resize(vector_size);
                for(std::size_t i=0; i<vector_size; ++i) range[i]=i;
                it_begin = range.begin();
                it_end = range.end() - 1;
                prefetcher_distance_factor = p_factor;
                chunk_size = p_factor * 64 / sizeof(T);
                m = l;
                range_size = vector_size;
            }

            explicit prefetcher_context (std::size_t begin, std::size_t end,
                std::initializer_list< T * > &&l)
            {
                std::size_t vector_size = end - begin;
                range.resize(vector_size);
                for(std::size_t i=0; i<vector_size; ++i) range[i] = i + begin;
                it_begin = range.begin();
                it_end = range.begin() + vector_size - 1;
                chunk_size = 64 / sizeof(T);
                m = l;
                range_size = vector_size;
            }

            prefetching_iterator<T> begin()
            {
                return prefetching_iterator<T>(0ul, it_begin, chunk_size, range_size, m);
            }

            prefetching_iterator<T> end()
            {
                return prefetching_iterator<T>(std::distance(it_begin, it_end + 1) , it_end, chunk_size, range_size, m);
            }

        };


        //function which initialize prefetcher_context
        template<typename T>
        prefetcher_context<T> make_prefetcher_context(std::size_t idx_begin, std::size_t idx_end,
            std::initializer_list< T * > &&l, std::size_t p_factor = 0)
        {
            if(p_factor == 0)
                return prefetcher_context<T>(idx_begin, idx_end, std::move(l));
            else
                return prefetcher_context<T>(idx_begin, idx_end, p_factor, std::move(l));
        }


        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position.
        template <typename Iterator>
        struct loop_n
        {
            ///////////////////////////////////////////////////////////////////
            // handle sequences of non-futures
            template <typename Iter, typename F>
            static Iter call(Iter it, std::size_t count, F && f)
            {
                for (/**/; count != 0; (void) --count, ++it)
                    f(it);
                return it;
            }

            template <typename Iter, typename CancelToken, typename F>
            static Iter call(Iter it, std::size_t count, CancelToken& tok,
                F && f)
            {
                for (/**/; count != 0; (void) --count, ++it)
                {
                    if (tok.was_cancelled())
                        break;
                    f(it);
                }
                return it;
            }
        };

        template <typename T>
        struct loop_n <prefetching_iterator<T>>
        {
            ///////////////////////////////////////////////////////////////////
            // handle sequences of non-futures when using prefetching
            template <typename F>
            static prefetching_iterator<T> call(prefetching_iterator<T> it, std::size_t count, F && f)
            {
                for (/**/; count != 0; (void) --count, ++it)
                {
                    std::vector<std::size_t>::iterator inner_it = it.base;
                    std::size_t j = it.idx;

                    for(/**/; j < std::min( it.range_size,(it.idx + it.chunk_size) ); ++j)
                    {
                        f(inner_it);
                        ++inner_it;
                    }

                    if(j < it.range_size - 1  )
                        for (auto& x: it.M_)
                            _mm_prefetch(((char*)(&x[j+1])), _MM_HINT_T0);
                }

                return it;
            }

            template <typename CancelToken, typename F>
            static prefetching_iterator<T> call(prefetching_iterator<T> it, std::size_t count, CancelToken& tok,
                F && f)
            {
                for (/**/; count != 0; (void) --count, ++it)
                {
                    std::vector<std::size_t>::iterator inner_it = it.base;
                    for(std::size_t j = it.idx;j < std::min( it.range_size,(it.idx + it.chunk_size) ); ++j)
                    {
                        if (tok.was_cancelled())
                            break;
                        f(inner_it);
                        ++inner_it;
                    }
                }

                return it;
            }

        };


    }

    template <typename Iter>
    struct loop_n_iterator_mapping
    {
        typedef Iter type;
    };

    template <typename T>
    struct loop_n_iterator_mapping<detail::prefetching_iterator<T> >
    {
        using type = typename detail::prefetching_iterator<T>::base_iterator;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename F>
    HPX_FORCEINLINE Iter
    loop_n(Iter it, std::size_t count, F && f)
    {
        return detail::loop_n<Iter>::call(it, count, std::forward<F>(f));
    }

    template <typename Iter, typename CancelToken, typename F>
    HPX_FORCEINLINE Iter
    loop_n(Iter it, std::size_t count, CancelToken& tok, F && f)
    {
        return detail::loop_n<Iter>::call(it, count, tok, std::forward<F>(f));
    };

    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position. If an exception is thrown,
        // the given cleanup function will be called.
        template <typename IterCat>
        struct loop_with_cleanup
        {
            ///////////////////////////////////////////////////////////////////
            template <typename FwdIter, typename F, typename Cleanup>
            static FwdIter call(FwdIter it, FwdIter last, F && f,
                Cleanup && cleanup)
            {
                FwdIter base = it;
                try {
                    for (/**/; it != last; ++it)
                        f(it);
                    return it;
                }
                catch (...) {
                    for (/**/; base != it; ++base)
                        cleanup(base);
                    throw;
                }
            }

            template <typename Iter, typename FwdIter, typename F,
                typename Cleanup>
            static FwdIter call(Iter it, Iter last, FwdIter dest, F && f,
                Cleanup && cleanup)
            {
                FwdIter base = dest;
                try {
                    for (/**/; it != last; (void) ++it, ++dest)
                        f(it, dest);
                    return dest;
                }
                catch (...) {
                    for (/**/; base != dest; ++base)
                        cleanup(base);
                    throw;
                }
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename F, typename Cleanup>
    HPX_FORCEINLINE Iter
    loop_with_cleanup(Iter it, Iter last, F && f, Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup<cat>::call(it, last,
            std::forward<F>(f), std::forward<Cleanup>(cleanup));
    }

    template <typename Iter, typename FwdIter, typename F, typename Cleanup>
    HPX_FORCEINLINE FwdIter
    loop_with_cleanup(Iter it, Iter last, FwdIter dest, F && f,
        Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup<cat>::call(it, last, dest,
            std::forward<F>(f), std::forward<Cleanup>(cleanup));
    }

    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position.
        template <typename IterCat>
        struct loop_with_cleanup_n
        {
            ///////////////////////////////////////////////////////////////////
            template <typename FwdIter, typename F, typename Cleanup>
            static FwdIter call(FwdIter it, std::size_t count, F && f,
                Cleanup && cleanup)
            {
                FwdIter base = it;
                try {
                    for (/**/; count != 0; (void) --count, ++it)
                        f(it);
                    return it;
                }
                catch (...) {
                    for (/**/; base != it; ++base)
                        cleanup(base);
                    throw;
                }
            }

            template <typename Iter, typename FwdIter, typename F,
                typename Cleanup>
            static FwdIter call(Iter it, std::size_t count, FwdIter dest,
                F && f, Cleanup && cleanup)
            {
                FwdIter base = dest;
                try {
                    for (/**/; count != 0; (void) --count, ++it, ++dest)
                        f(it, dest);
                    return dest;
                }
                catch (...) {
                    for (/**/; base != dest; ++base)
                        cleanup(base);
                    throw;
                }
            }

            ///////////////////////////////////////////////////////////////////
            template <typename FwdIter, typename CancelToken, typename F,
                typename Cleanup>
            static FwdIter call_with_token(FwdIter it, std::size_t count,
                CancelToken& tok, F && f, Cleanup && cleanup)
            {
                FwdIter base = it;
                try {
                    for (/**/; count != 0; (void) --count, ++it)
                    {
                        if (tok.was_cancelled())
                            break;
                        f(it);
                    }
                    return it;
                }
                catch (...) {
                    tok.cancel();
                    for (/**/; base != it; ++base)
                        cleanup(base);
                    throw;
                }
            }

            template <typename Iter, typename FwdIter, typename CancelToken,
                typename F, typename Cleanup>
            static FwdIter call_with_token(Iter it, std::size_t count,
                FwdIter dest, CancelToken& tok, F && f, Cleanup && cleanup)
            {
                FwdIter base = dest;
                try {
                    for (/**/; count != 0; (void) --count, ++it, ++dest)
                    {
                        if (tok.was_cancelled())
                            break;
                        f(it, dest);
                    }
                    return dest;
                }
                catch (...) {
                    tok.cancel();
                    for (/**/; base != dest; ++base)
                        cleanup(base);
                    throw;
                }
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename F, typename Cleanup>
    HPX_FORCEINLINE Iter
    loop_with_cleanup_n(Iter it, std::size_t count, F && f, Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup_n<cat>::call(it, count,
            std::forward<F>(f), std::forward<Cleanup>(cleanup));
    }

    template <typename Iter, typename FwdIter, typename F, typename Cleanup>
    HPX_FORCEINLINE FwdIter
    loop_with_cleanup_n(Iter it, std::size_t count, FwdIter dest, F && f,
        Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup_n<cat>::call(it, count, dest,
            std::forward<F>(f), std::forward<Cleanup>(cleanup));
    }

    template <typename Iter, typename CancelToken, typename F, typename Cleanup>
    HPX_FORCEINLINE Iter
    loop_with_cleanup_n_with_token(Iter it, std::size_t count,
        CancelToken& tok, F && f, Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup_n<cat>::call_with_token(it, count,
            tok, std::forward<F>(f), std::forward<Cleanup>(cleanup));
    };

    template <typename Iter, typename FwdIter, typename CancelToken,
        typename F, typename Cleanup>
    HPX_FORCEINLINE FwdIter
    loop_with_cleanup_n_with_token(Iter it, std::size_t count, FwdIter dest,
        CancelToken& tok, F && f, Cleanup && cleanup)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_with_cleanup_n<cat>::call_with_token(it, count,
            dest, tok, std::forward<F>(f), std::forward<Cleanup>(cleanup));
    };

    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position.
        template <typename IterCat>
        struct loop_idx_n
        {
            ///////////////////////////////////////////////////////////////////
            // handle sequences of non-futures
            template <typename Iter, typename F>
            static Iter
            call(std::size_t base_idx, Iter it, std::size_t count, F && f)
            {
                for (/**/; count != 0; (void) --count, ++it, ++base_idx)
                    f(*it, base_idx);

                return it;
            }

            template <typename Iter, typename CancelToken, typename F>
            static Iter
            call(std::size_t base_idx, Iter it, std::size_t count,
                CancelToken& tok, F && f)
            {
                for (/**/; count != 0; (void) --count, ++it, ++base_idx)
                {
                    if (tok.was_cancelled(base_idx))
                        break;
                    f(*it, base_idx);
                }
                return it;
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename F>
    HPX_FORCEINLINE Iter
    loop_idx_n(std::size_t base_idx, Iter it, std::size_t count, F && f)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_idx_n<cat>::call(base_idx, it, count,
            std::forward<F>(f));
    }

    template <typename Iter, typename CancelToken, typename F>
    HPX_FORCEINLINE Iter
    loop_idx_n(std::size_t base_idx, Iter it, std::size_t count,
        CancelToken& tok, F && f)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::loop_idx_n<cat>::call(base_idx, it, count, tok,
            std::forward<F>(f));
    };

    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position.
        template <typename IterCat>
        struct accumulate_n
        {
            template <typename Iter, typename T, typename Pred>
            static T call(Iter it, std::size_t count, T init, Pred && f)
            {
                for (/**/; count != 0; (void) --count, ++it)
                    init = f(init, *it);
                return init;
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename T, typename Pred>
    HPX_FORCEINLINE T
    accumulate_n(Iter it, std::size_t count, T init, Pred && f)
    {
        typedef typename std::iterator_traits<Iter>::iterator_category cat;
        return detail::accumulate_n<cat>::call(it, count, std::move(init),
            std::forward<Pred>(f));
    }
}}}

#endif
