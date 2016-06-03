//  Copyright (c) 2007-2016 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_PARALLEL_UTIL_LOOP_MAY_27_2014_1040PM)
#define HPX_PARALLEL_UTIL_LOOP_MAY_27_2014_1040PM

#include <hpx/config.hpp>
#include <hpx/parallel/util/cancellation_token.hpp>

#include <iterator>
#include <algorithm>
#include <initializer_list>
#include <vector>

namespace hpx { namespace parallel { namespace util
{
    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        ///////////////////////////////////////////////////////////////////////
        // Helper class to repeatedly call a function starting from a givena
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
        // Helper class to repeatedly call a function a given number of times
        // starting from a given iterator position.
        template <typename Iterator>
        struct loop_n
        {
         /////////////////////////////////////////////////////////////////
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

        template <typename data_type>
        struct loop_n <prefetching_iterator<data_type>>
        {
            /////////////////////////////////////////////////////////////////
            // handle sequences of non-futures when using prefetching
            template <typename F>
            static prefetching_iterator<data_type> call(prefetching_iterator<data_type> it,
                std::size_t count, F && f)
            {
                std::size_t cnt = count + 1;
                for (/**/; cnt != 0; (void) --cnt, ++it)
                {
                    auto inner_it = it.base;////
                    std::size_t j = it.idx;
                    std::size_t last = it.idx + it.chunk_size;

                    if (it.range_size < last)
                        last = it.range_size;

                    for(/**/; j < last ; ++j)
                    {
                        f(inner_it);
                        ++inner_it;
                    }
                    if(j < it.range_size - 1)
                        for (auto& x: it.M_)
                            _mm_prefetch(((char*)(&x[j+1])), _MM_HINT_T0);
                }
                return it;
            }

            template <typename CancelToken, typename F>
            static prefetching_iterator<data_type> call(prefetching_iterator<data_type> it,
                std::size_t count, CancelToken& tok,
                F && f)
            {
                std::size_t cnt = count + 1;
                for (/**/; cnt != 0; (void) --cnt, ++it)
                {
                    auto inner_it = it.base;////
                    std::size_t j = it.idx;
                    std::size_t last = it.idx + it.chunk_size;

                    if (it.range_size < last)
                        last = it.range_size;

                    for(/**/; j < last; ++j)
                    {
                        if (tok.was_cancelled())
                            break;
                        f(inner_it);
                        ++inner_it;
                        if(j < it.range_size - 1)
                            for (auto& x: it.M_)
                                _mm_prefetch(((char*)(&x[j+1])), _MM_HINT_T0);
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
    struct loop_n_iterator_mapping<detail::prefetching_iterator<T>>
    {
        typedef typename  detail::prefetching_iterator<T>::base_iterator type;
    };
    ///////////////////////////////////////////////////////////////////////////
    template <typename Iter, typename F>
    HPX_FORCEINLINE Iter
    loop_n(Iter it, std::size_t count, F && f)
    {
        typedef typename util::loop_n_iterator_mapping<
                                Iter>::type iterator_type;

        return detail::loop_n<iterator_type>::call(it, count, std::forward<F>(f));
    }

    template <typename Iter, typename CancelToken, typename F>
    HPX_FORCEINLINE Iter
    loop_n(Iter it, std::size_t count, CancelToken& tok, F && f)
    {
        typedef typename util::loop_n_iterator_mapping<
                                Iter>::type iterator_type;

        return detail::loop_n<iterator_type>::call(it, count, tok, std::forward<F>(f));
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

