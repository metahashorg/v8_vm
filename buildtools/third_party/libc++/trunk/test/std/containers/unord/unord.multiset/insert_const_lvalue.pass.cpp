//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_set>

// template <class Value, class Hash = hash<Value>, class Pred = equal_to<Value>,
//           class Alloc = allocator<Value>>
// class unordered_multiset

// iterator insert(const value_type& x);

#include <unordered_set>
#include <cassert>

#include "min_allocator.h"

template<class Container>
void do_insert_const_lvalue_test()
{
    typedef Container C;
    typedef typename C::iterator R;
    typedef typename C::value_type VT;
    C c;
    const VT v1(3.5);
    R r = c.insert(v1);
    assert(c.size() == 1);
    assert(*r == 3.5);

    r = c.insert(v1);
    assert(c.size() == 2);
    assert(*r == 3.5);

    const VT v2(4.5);
    r = c.insert(v2);
    assert(c.size() == 3);
    assert(*r == 4.5);

    const VT v3(5.5);
    r = c.insert(v3);
    assert(c.size() == 4);
    assert(*r == 5.5);
}

int main()
{
    do_insert_const_lvalue_test<std::unordered_multiset<double> >();
#if TEST_STD_VER >= 11
    {
        typedef std::unordered_multiset<double, std::hash<double>,
            std::equal_to<double>, min_allocator<double>> C;
        do_insert_const_lvalue_test<C>();
    }
#endif
}
