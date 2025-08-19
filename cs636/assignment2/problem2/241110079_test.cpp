#include <iostream>
#include <gtest/gtest.h>
#include "241110079.h"
#include <string>
using namespace std;
TEST(QueueTest, Test_Enq){
    ConcurrentQueue Q;
    const char* expected = "10 20 30 40";
    Q.enq(10);
    Q.enq(20);
    Q.enq(30);
    Q.enq(40);
    const char* values = Q.toString().c_str();
    EXPECT_STREQ(expected, values);

}
TEST(QueueTest, Test_Deq){
    ConcurrentQueue Q;
    int temp;
    const char* expected = "30 40";
    Q.enq(10);
    Q.enq(20);
    Q.enq(30);
    Q.enq(40);
    Q.deq(temp);
    EXPECT_EQ(temp, 10);
    Q.deq(temp);
    EXPECT_EQ(temp, 20);
    const char* values = Q.toString().c_str();
    EXPECT_STREQ(expected, values);

}
TEST(QueueTest, Test_Deq_from_Empty_Q){
    ConcurrentQueue Q;
    int temp;
    const char* expected = "";
    Q.deq(temp);
    const char* values = Q.toString().c_str();
    EXPECT_STREQ(expected, values);

}
int main(int argc, char **argv){
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();

}