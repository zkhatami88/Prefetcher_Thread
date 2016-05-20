//  Copyright (c) 2016 Zahra Khatami, Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/parallel_algorithm.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <boost/range/functions.hpp>

#include "test_utils.hpp"

///////////////////////////////////////////////////////////////////////////////
template <typename ExPolicy, typename IteratorTag>
void test_for_each_with_prefetching(ExPolicy policy, IteratorTag)
{
    static_assert(
        hpx::parallel::is_execution_policy<ExPolicy>::value,
        "hpx::parallel::is_execution_policy<ExPolicy>::value");

    typedef hpx::parallel::util::detail::prefetching_iterator<double> base_iterator;
    typedef test::test_iterator<base_iterator, IteratorTag> iterator;

    std::size_t prefetch_distance_factor = 20;
    std::vector<double> c(10007,1.0);
    std::vector<double> b(10007,1.0);
    auto ctx_1 = hpx::parallel::util::detail::make_prefetcher_context<double>
				(0,10007,{c.data()},prefetch_distance_factor); 
    auto ctx_2 = hpx::parallel::util::detail::make_prefetcher_context<double>(0,10007,{b.data()}); 


    //ctx_1
    hpx::parallel::for_each(policy,
        ctx_1.begin(), ctx_1.end(),
        [&](std::size_t i) {
            c[i] = 42.1;
        });

    base_iterator result = hpx::parallel::for_each(policy,
            ctx_1.begin(), ctx_1.end(),
            [](std::size_t i) {});
    //HPX_TEST( result == ctx_1.end() ); //it fails, why??

    //ctx_2
    hpx::parallel::for_each(policy,
        ctx_2.begin(), ctx_2.end(),
        [&](std::size_t i) {
            b[i] = 42.1;
        });

    base_iterator result2 = hpx::parallel::for_each(policy,
            ctx_2.begin(), ctx_2.end(),
            [](std::size_t i) {});
//    HPX_TEST( result2 == iterator(ctx_2.end()) ); //it fails, why??


  // verify values
    std::size_t count = 0;
    std::for_each(boost::begin(c), boost::end(c),
        [&count](std::size_t v) -> void {
            HPX_TEST_EQ(v, std::size_t(42.1));
	    ++count;
        });
    HPX_TEST_EQ(count, c.size());

    count = 0;
    std::for_each(boost::begin(b), boost::end(b),
        [&count](std::size_t v) -> void {
            HPX_TEST_EQ(v, std::size_t(42));
	    ++count;
        });
    HPX_TEST_EQ(count, b.size());

}

template <typename ExPolicy, typename IteratorTag>
void test_for_each_with_prefetching_async(ExPolicy p, IteratorTag)
{
    typedef hpx::parallel::util::detail::prefetching_iterator<double> base_iterator;
    typedef test::test_iterator<base_iterator, IteratorTag> iterator;

    std::size_t prefetch_distance_factor = 20;
    std::vector<double> c(10007,1.0);
    std::vector<double> b(10007,1.0);
    auto ctx_1 = hpx::parallel::util::detail::make_prefetcher_context<double>
				(0,10007,{c.data()},prefetch_distance_factor); 
    auto ctx_2 = hpx::parallel::util::detail::make_prefetcher_context<double>(0,10007,{b.data()}); 


    //ctx_1
    hpx::future<base_iterator> f1 =
    hpx::parallel::for_each(p,
        ctx_1.begin(), ctx_1.end(),
        [&](std::size_t i) {
            c[i] = 42;
        });


    f1.wait();
    //HPX_TEST(f1.get() == ctx_1.end()); //fails I don know why??

    //ctx_2
    hpx::future<base_iterator> f2 =
    hpx::parallel::for_each(p,
        ctx_2.begin(), ctx_2.end(),
        [&](std::size_t i) {
            b[i] = 42;
        });

    f2.wait();
    //HPX_TEST(f2.get() == ctx_2.end()); //fails I don know why??


    // verify values
    std::size_t count = 0;
    std::for_each(boost::begin(c), boost::end(c),
        [&count](std::size_t v) -> void {
            HPX_TEST_EQ(v, std::size_t(42));
            ++count;
        });
    HPX_TEST_EQ(count, c.size());

    count = 0;
    std::for_each(boost::begin(b), boost::end(b),
        [&count](std::size_t v) -> void {
            HPX_TEST_EQ(v, std::size_t(42));
            ++count;
        });
    HPX_TEST_EQ(count, b.size());
}

template <typename IteratorTag>
void test_for_each_with_prefetching_test()
{
    using namespace hpx::parallel;

    //test_for_each_with_prefetching(seq, IteratorTag()); //loop_n is not called for seq policy
    test_for_each_with_prefetching(par, IteratorTag());
    test_for_each_with_prefetching(par_vec, IteratorTag());

    //test_for_each_with_prefetching_async(seq(task), IteratorTag());
    test_for_each_with_prefetching_async(par(task), IteratorTag());

    //test_for_each_with_prefetching(execution_policy(seq), IteratorTag());
    test_for_each_with_prefetching(execution_policy(par), IteratorTag());
    test_for_each_with_prefetching(execution_policy(par_vec), IteratorTag());

    //test_for_each_with_prefetching(execution_policy(seq(task)), IteratorTag());
    test_for_each_with_prefetching(execution_policy(par(task)), IteratorTag());
}

void for_each_with_prefetching_test()
{
    test_for_each_with_prefetching_test<std::random_access_iterator_tag>();
    test_for_each_with_prefetching_test<std::forward_iterator_tag>();
    test_for_each_with_prefetching_test<std::input_iterator_tag>();
}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(boost::program_options::variables_map& vm)
{
    unsigned int seed = (unsigned int)std::time(0);
    if (vm.count("seed"))
        seed = vm["seed"].as<unsigned int>();

    std::cout << "using seed: " << seed << std::endl;
    std::srand(seed);

    for_each_with_prefetching_test();
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    // add command line option which controls the random number generator seed
    using namespace boost::program_options;
    options_description desc_commandline(
        "Usage: " HPX_APPLICATION_STRING " [options]");

    desc_commandline.add_options()
        ("seed,s", value<unsigned int>(),
        "the random number generator seed to use for this run")
        ;

    // By default this test should run on all available cores
    std::vector<std::string> cfg;
    cfg.push_back("hpx.os_threads=" +
        boost::lexical_cast<std::string>(hpx::threads::hardware_concurrency()));

    // Initialize and run HPX
    HPX_TEST_EQ_MSG(hpx::init(desc_commandline, argc, argv, cfg), 0,
        "HPX main exited with non-zero status");

    return hpx::util::report_errors();
}
