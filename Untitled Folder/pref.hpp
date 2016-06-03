//  Copyright (c) 2016 Zahra Khatami
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
	namespace detail
    {        //New random access iterator which is used for prefetching
        //all containers within lambda functions
        template<typename data_type>
        class prefetching_iterator
        : public std::iterator<std::random_access_iterator_tag, std::size_t>
        {
            public:
            typedef typename std::vector<std::size_t>::iterator base_iterator;
            //std::vector< data_type * > M_;
            std::tuple<data_type &> M;
            base_iterator base;
            std::size_t chunk_size;
            std::size_t range_size;
            std::size_t idx;

            explicit prefetching_iterator(typename Itr_type, std::size_t idx_,
                base_iterator base_ , std::size_t chunk_size_,
                std::size_t range_size_, std::tuple<data_type> & A_)
            {
            	typedef typename Itr_type base_iterator;
            	M_ = A;
            	base = base_;
            	chunk_size = chunk_size_;
            	range_size = range_size_;
            	M = A_;
            	idx = idx_;
            }

            typedef  typename std::iterator<std::random_access_iterator_tag,
            std::size_t>::difference_type difference_type;

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
            inline difference_type
            operator-(const prefetching_iterator& rhs) const
            {
                return (idx-rhs.idx)/chunk_size;
            }
            inline prefetching_iterator
            operator+(difference_type rhs) const
            {
                return prefetching_iterator((idx+(rhs*chunk_size)),
                    (base+(rhs*chunk_size)),chunk_size,range_size,M_);
            }
            inline prefetching_iterator
            operator-(difference_type rhs) const
            {
                return prefetching_iterator((idx-(rhs*chunk_size)),
                    (base-(rhs*chunk_size)),chunk_size,range_size,M_);
            }
            friend inline prefetching_iterator
            operator+(difference_type lhs,
                const prefetching_iterator& rhs)
            {
                return rhs + lhs;
            }
            friend inline prefetching_iterator
            operator-(difference_type lhs,
                const prefetching_iterator& rhs)
            {
                return lhs - rhs;
            }
            inline bool operator==(const prefetching_iterator& rhs) const
            {
                return idx == rhs.idx;
            }
            inline bool operator!=(const prefetching_iterator& rhs) const
            {
                return idx != rhs.idx;
            }
            inline bool operator>(const prefetching_iterator& rhs) const
            {
                return idx > rhs.idx;
            }
            inline bool operator<(const prefetching_iterator& rhs) const
            {
                return idx < rhs.idx;
            }
            inline bool operator>=(const prefetching_iterator& rhs) const
            {
                return idx >= rhs.idx;
            }
            inline bool operator<=(const prefetching_iterator& rhs) const
            {
                return idx <= rhs.idx;
            }
            inline std::size_t operator*() const {return idx;}
        };

        ////////////////////////////////////////////////////////////////////

        //Helper class to initialize prefetching_iterator
        template<typename data_type, typename Itr>
        struct prefetcher_context
        {
            Itr it_begin;
            Itr it_end;
            std::size_t chunk_size;
            std::tuple<data_type &> m;
            //std::vector< data_type * > m;
            std::size_t range_size;

            explicit prefetcher_context (Itr begin, Itr end,
                std::size_t p_factor, std::tuple<data_type &> &&l) //std::initializer_list< T * > &&l
            {
                it_begin = begin;
                it_end = end;
                chunk_size = p_factor * 64ul / sizeof(data_type);
                m = l;
                range_size = std::difference(begin, end);
            }

            explicit prefetcher_context (Itr begin, Itr end,
                std::initializer_list< T * > &&l)
            {
                it_begin = begin;
                it_end = end;
                chunk_size = 64ul / sizeof(data_type);
                m = l;
                range_size = std::difference(begin, end);
            }

            prefetching_iterator<data_type> begin()
            {
                return prefetching_iterator<data_type>(Itr, 0ul, it_begin,
                    chunk_size, range_size, m);
            }

            prefetching_iterator<data_type> end()
            {
                return prefetching_iterator<data_type>(Itr, range_size, it_end,
                    chunk_size, range_size, m);
            }

        };

        //function which initialize prefetcher_context
        template<typename data_type, typename Itr>
        prefetcher_context<data_type, Itr> make_prefetcher_context(Itr idx_begin,
            Itr idx_end, std::tuple<data_type &> l,//std::initializer_list< data_type * > &&l,
            std::size_t p_factor = 0)
        {
            if(p_factor == 0)
                return prefetcher_context<data_type, Itr>(idx_begin, idx_end,
                    std::move(l));
            else
                return prefetcher_context<data_type, Itr>(idx_begin, idx_end,
                    p_factor, std::move(l));
        }




    }
}}}

#endif

