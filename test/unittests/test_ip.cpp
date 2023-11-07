#include "test_common.h"
#include <iostream>

#include <openvpn/common/size.hpp>
#include <openvpn/common/exception.hpp>

#include <openvpn/ip/ping6.hpp>
#include <openvpn/addr/ip.hpp>
#include <openvpn/addr/pool.hpp>
#include <openvpn/addr/ipv6.hpp>

using namespace openvpn;

static const uint8_t icmp6_packet[] = {
    // clang-format off
    0x60, 0x06, 0x22, 0xe5, 0x00, 0x40, 0x3a, 0x28, 0x26, 0x01, 0x02, 0x81, 0x84, 0x80, 0x14, 0xe0,
    0xbc, 0xc1, 0x91, 0x20, 0xfc, 0xa3, 0x0e, 0x22, 0x26, 0x00, 0x1f, 0x18, 0x47, 0x2b, 0x89, 0x05,
    0x2a, 0xc4, 0x3b, 0xf3, 0xd5, 0x77, 0x29, 0x42, 0x80, 0x00, 0x99, 0x99, 0x3f, 0xd4, 0x00, 0x0e,
    0x43, 0xd4, 0xc3, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x3d, 0xc2, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    // clang-format on

};

static bool verbose = false;

TEST(IPAddr, icmp6csum)
{
    const ICMPv6 *icmp = (const ICMPv6 *)icmp6_packet;
    const size_t len = sizeof(icmp6_packet);

    if (verbose)
    {
        std::cout << "From : " << IPv6::Addr::from_in6_addr(&icmp->head.saddr).to_string() << std::endl;
        std::cout << "To   : " << IPv6::Addr::from_in6_addr(&icmp->head.daddr).to_string() << std::endl;
    }
    const std::uint16_t csum = Ping6::csum_icmp(icmp, len);
    if (verbose)
    {
        std::cout << "Checksum: " << csum << std::endl;
    }
    ASSERT_TRUE(csum == 0) << "checksum=" << csum << " but should be zero";
}

TEST(IPAddr, pool)
{
    IP::Pool pool;
    pool.add_range(IP::Range(IP::Addr::from_string("1.2.3.4"), 16));
    pool.add_range(IP::Range(IP::Addr::from_string("Fe80::23a1:b152"), 4));
    pool.add_addr(IP::Addr::from_string("10.10.1.1"));
    ASSERT_TRUE(pool.acquire_specific_addr(IP::Addr::from_string("1.2.3.10")));

    std::stringstream s;
    for (int i = 0;; ++i)
    {
        IP::Addr addr;
        if (i == 7)
        {
            pool.release_addr(IP::Addr::from_string("1.2.3.7"));
        }
        else if (i == 11)
        {
            pool.release_addr(IP::Addr::from_string("1.2.3.3"));
            pool.release_addr(IP::Addr::from_string("1.2.3.4"));
            pool.release_addr(IP::Addr::from_string("1.2.3.5"));
        }
        else
        {
            if (pool.acquire_addr(addr))
            {
                s << addr << " (" << pool.n_in_use() << ")" << std::endl;
            }
            else
                break;
        }
    }
    ASSERT_EQ("1.2.3.4 (1)\n"
              "1.2.3.5 (2)\n"
              "1.2.3.6 (3)\n"
              "1.2.3.7 (4)\n"
              "1.2.3.8 (5)\n"
              "1.2.3.9 (6)\n"
              "1.2.3.11 (8)\n"
              "1.2.3.12 (8)\n"
              "1.2.3.13 (9)\n"
              "1.2.3.14 (10)\n"
              "1.2.3.15 (9)\n"
              "1.2.3.16 (10)\n"
              "1.2.3.17 (11)\n"
              "1.2.3.18 (12)\n"
              "1.2.3.19 (13)\n"
              "fe80::23a1:b152 (14)\n"
              "fe80::23a1:b153 (15)\n"
              "fe80::23a1:b154 (16)\n"
              "fe80::23a1:b155 (17)\n"
              "10.10.1.1 (18)\n"
              "1.2.3.7 (19)\n"
              "1.2.3.4 (20)\n"
              "1.2.3.5 (21)\n",
              s.str());
}

struct test_case
{
    int shift;
    uint8_t ip[16];
};



void do_shift_tests(std::vector<test_case> test_vectors, bool leftshift)
{
    sockaddr_in6 sa{};

    for (int i = 0; i < 16; i++)
    {
        /* first vector has to use a shift of 0  */
        sa.sin6_addr.s6_addr[i] = test_vectors[0].ip[i];
    }

    for (auto &t : test_vectors)
    {
        // Shift by zero should not change anything
        auto addr = IPv6::Addr::from_sockaddr(&sa);

        IPv6::Addr shifted_addr{};

        if (leftshift)
            shifted_addr = addr << t.shift;
        else
            shifted_addr = addr >> t.shift;
        auto ret = shifted_addr.to_sockaddr();

        sockaddr_in6 cmp{};
        cmp.sin6_family = AF_INET6;
        for (int i = 0; i < 16; i++)
        {
            cmp.sin6_addr.s6_addr[i] = t.ip[i];
        }

        if (memcmp(&cmp, &ret, sizeof(sockaddr_in6)) != 0)
            std::cout << "BROKEN " << std::to_string(t.shift) << std::endl;

        EXPECT_EQ(memcmp(&cmp, &ret, sizeof(sockaddr_in6)), 0);
    }
}


/* test vectors are generated with gen_ip_shifts.py */
TEST(IPAddr, left_shift)
{
    std::vector<test_case> tests{{0, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x0, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}},
                                 {1, {0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xef, 0x11, 0x32, 0x1, 0x55, 0x77, 0x99, 0xbb, 0xdd, 0xfe}},
                                 {31, {0x2a, 0xb3, 0x3b, 0xc4, 0x4c, 0x80, 0x55, 0x5d, 0xe6, 0x6e, 0xf7, 0x7f, 0x80, 0x0, 0x0, 0x0}},
                                 {32, {0x55, 0x66, 0x77, 0x88, 0x99, 0x0, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x0, 0x0, 0x0, 0x0}},
                                 {33, {0xaa, 0xcc, 0xef, 0x11, 0x32, 0x1, 0x55, 0x77, 0x99, 0xbb, 0xdd, 0xfe, 0x0, 0x0, 0x0, 0x0}},
                                 {45, {0xce, 0xf1, 0x13, 0x20, 0x15, 0x57, 0x79, 0x9b, 0xbd, 0xdf, 0xe0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {63, {0x4c, 0x80, 0x55, 0x5d, 0xe6, 0x6e, 0xf7, 0x7f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {64, {0x99, 0x0, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {67, {0xc8, 0x5, 0x55, 0xde, 0x66, 0xef, 0x77, 0xf8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {80, {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {97, {0x99, 0xbb, 0xdd, 0xfe, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {127, {0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {128, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}};
    do_shift_tests(tests, true);
}

TEST(IPAddr, left_shift_random)
{
    std::vector<test_case> tests{
        {0, {0xbc, 0x46, 0xc, 0xcb, 0x8f, 0x85, 0x25, 0x9a, 0x74, 0x91, 0xd4, 0x80, 0xed, 0x2d, 0xe8, 0xe0}},
        {1, {0x78, 0x8c, 0x19, 0x97, 0x1f, 0xa, 0x4b, 0x34, 0xe9, 0x23, 0xa9, 0x1, 0xda, 0x5b, 0xd1, 0xc0}},
        {31, {0xc7, 0xc2, 0x92, 0xcd, 0x3a, 0x48, 0xea, 0x40, 0x76, 0x96, 0xf4, 0x70, 0x0, 0x0, 0x0, 0x0}},
        {32, {0x8f, 0x85, 0x25, 0x9a, 0x74, 0x91, 0xd4, 0x80, 0xed, 0x2d, 0xe8, 0xe0, 0x0, 0x0, 0x0, 0x0}},
        {33, {0x1f, 0xa, 0x4b, 0x34, 0xe9, 0x23, 0xa9, 0x1, 0xda, 0x5b, 0xd1, 0xc0, 0x0, 0x0, 0x0, 0x0}},
        {45, {0xa4, 0xb3, 0x4e, 0x92, 0x3a, 0x90, 0x1d, 0xa5, 0xbd, 0x1c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {63, {0x3a, 0x48, 0xea, 0x40, 0x76, 0x96, 0xf4, 0x70, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {64, {0x74, 0x91, 0xd4, 0x80, 0xed, 0x2d, 0xe8, 0xe0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {67, {0xa4, 0x8e, 0xa4, 0x7, 0x69, 0x6f, 0x47, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {80, {0xd4, 0x80, 0xed, 0x2d, 0xe8, 0xe0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {97, {0xda, 0x5b, 0xd1, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {127, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {128, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}};
    do_shift_tests(tests, true);
}

TEST(IPAddr, right_shift)
{
    std::vector<test_case> tests{
        {0, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x0, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}},
        {1, {0x8, 0x91, 0x19, 0xa2, 0x2a, 0xb3, 0x3b, 0xc4, 0x4c, 0x80, 0x55, 0x5d, 0xe6, 0x6e, 0xf7, 0x7f}},
        {31, {0x0, 0x0, 0x0, 0x0, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xef, 0x11, 0x32, 0x1, 0x55, 0x77}},
        {32, {0x0, 0x0, 0x0, 0x0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x0, 0xaa, 0xbb}},
        {33, {0x0, 0x0, 0x0, 0x0, 0x8, 0x91, 0x19, 0xa2, 0x2a, 0xb3, 0x3b, 0xc4, 0x4c, 0x80, 0x55, 0x5d}},
        {45, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x89, 0x11, 0x9a, 0x22, 0xab, 0x33, 0xbc, 0x44, 0xc8, 0x5}},
        {63, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xef, 0x11}},
        {64, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}},
        {67, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x24, 0x46, 0x68, 0x8a, 0xac, 0xce, 0xf1}},
        {80, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66}},
        {97, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8, 0x91, 0x19, 0xa2}},
        {127, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
        {128, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}};
    do_shift_tests(tests, false);
}

TEST(IPAddr, right_shift_random)
{
    std::vector<test_case> tests{{0, {0x6d, 0xfb, 0x4a, 0x15, 0xb3, 0x6a, 0xd8, 0x25, 0x42, 0x83, 0x27, 0x83, 0xa9, 0x27, 0x2d, 0x3}},
                                 {1, {0x36, 0xfd, 0xa5, 0xa, 0xd9, 0xb5, 0x6c, 0x12, 0xa1, 0x41, 0x93, 0xc1, 0xd4, 0x93, 0x96, 0x81}},
                                 {31, {0x0, 0x0, 0x0, 0x0, 0xdb, 0xf6, 0x94, 0x2b, 0x66, 0xd5, 0xb0, 0x4a, 0x85, 0x6, 0x4f, 0x7}},
                                 {32, {0x0, 0x0, 0x0, 0x0, 0x6d, 0xfb, 0x4a, 0x15, 0xb3, 0x6a, 0xd8, 0x25, 0x42, 0x83, 0x27, 0x83}},
                                 {33, {0x0, 0x0, 0x0, 0x0, 0x36, 0xfd, 0xa5, 0xa, 0xd9, 0xb5, 0x6c, 0x12, 0xa1, 0x41, 0x93, 0xc1}},
                                 {45, {0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x6f, 0xda, 0x50, 0xad, 0x9b, 0x56, 0xc1, 0x2a, 0x14, 0x19}},
                                 {63, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xdb, 0xf6, 0x94, 0x2b, 0x66, 0xd5, 0xb0, 0x4a}},
                                 {64, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x6d, 0xfb, 0x4a, 0x15, 0xb3, 0x6a, 0xd8, 0x25}},
                                 {67, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xd, 0xbf, 0x69, 0x42, 0xb6, 0x6d, 0x5b, 0x4}},
                                 {80, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x6d, 0xfb, 0x4a, 0x15, 0xb3, 0x6a}},
                                 {97, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36, 0xfd, 0xa5, 0xa}},
                                 {127, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
                                 {128, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}};
    do_shift_tests(tests, false);
}

TEST(IPAddr, mapped_v4)
{
    IP::Addr v6mapped{"::ffff:2332:123a"};


    EXPECT_TRUE(v6mapped.is_mapped_address());
    IP::Addr notMapped = v6mapped.to_v4_addr();

    EXPECT_EQ(v6mapped.to_string(), "::ffff:35.50.18.58");
    EXPECT_EQ(notMapped.to_string(), "35.50.18.58");

    EXPECT_FALSE(IP::Addr{"::faff:2332:123a"}.is_mapped_address());
    EXPECT_FALSE(IP::Addr{"::2332:123a"}.is_mapped_address());
    EXPECT_FALSE(IP::Addr{"192.168.0.123"}.is_mapped_address());
}