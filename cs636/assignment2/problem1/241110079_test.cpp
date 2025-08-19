#include <iostream>
#include <sstream>
#include <gtest/gtest.h>
#include "241110079.h"
#include <string>
#include <gmock/gmock.h>
using namespace std;
TEST(HashTableTest, Test_Insert){
    HashTable ht;
    init_hash_table(&ht);
    bool* results = new bool[4];
    memset(results, 0, sizeof(bool) * 4);
    auto* h_kvs_insert = new KeyValue[4];
    memset(h_kvs_insert, 0, sizeof(KeyValue) * 4);
    for(int i=0; i<4; i++){
        h_kvs_insert[i].key = i;
        h_kvs_insert[i].value = i+1;
    }
    batch_insert(&ht, h_kvs_insert, results,4);
    std::string contents = toString(&ht);
    std::string expected[] = {"[Key: 0, Value: 1]", "[Key: 1, Value: 2]", "[Key: 2, Value: 3]", "[Key: 3, Value: 4]"};
    EXPECT_THAT(contents, ::testing::HasSubstr(expected[0]));
    EXPECT_EQ(results[0], true);
    EXPECT_THAT(contents, ::testing::HasSubstr(expected[1]));
    EXPECT_EQ(results[1], true);
    EXPECT_THAT(contents, ::testing::HasSubstr(expected[2]));
    EXPECT_EQ(results[2], true);
    EXPECT_THAT(contents, ::testing::HasSubstr(expected[3]));
    EXPECT_EQ(results[3], true);

}
TEST(HashTableTest, Test_Delete){
    HashTable ht;
    init_hash_table(&ht);
    bool* results = new bool[4];
    memset(results, 0, sizeof(bool) * 4);
    auto* h_kvs_insert = new KeyValue[4];
    memset(h_kvs_insert, 0, sizeof(KeyValue) * 4);
    for(int i=0; i<4; i++){
        h_kvs_insert[i].key = i;
        h_kvs_insert[i].value = i+1;
    }
    /* 2 values exist, 2 do not */
    uint32_t del_keys[4] = {1,5,3,8}; 
    batch_insert(&ht, h_kvs_insert, results,4);
    results = new bool[4];
    batch_delete(&ht, del_keys, results,4);
    std::string contents = toString(&ht);
    std::string expected[] = {"[Key: 0, Value: 1]", "[Key: 1, Value: 2]", "[Key: 2, Value: 3]", "[Key: 3, Value: 4]"};
    EXPECT_THAT(contents, ::testing::Not(::testing::HasSubstr(expected[1])));
    EXPECT_THAT(contents, ::testing::Not(::testing::HasSubstr(expected[3])));
    EXPECT_EQ(results[0], true);
    EXPECT_EQ(results[1], false);
    EXPECT_EQ(results[2], true);
    EXPECT_EQ(results[3], false);

}

TEST(HashTableTest, Test_Lookup){
    HashTable ht;
    init_hash_table(&ht);
    bool* results = new bool[4];
    memset(results, 0, sizeof(bool) * 4);
    auto* h_kvs_insert = new KeyValue[4];
    memset(h_kvs_insert, 0, sizeof(KeyValue) * 4);
    for(int i=0; i<4; i++){
        h_kvs_insert[i].key = i;
        h_kvs_insert[i].value = i+1;
    }
    /* 2 values exits, 2 do not */
    uint32_t lookup_keys[4] = {1,5,3,8}; 
    results = new bool[4];
    batch_insert(&ht, h_kvs_insert, results,4);
    uint32_t* values = new uint32_t[4];
    batch_lookup(&ht, lookup_keys, values,4);
    EXPECT_EQ(values[0], 2);
    EXPECT_EQ(values[1], -1);
    EXPECT_EQ(values[2], 4);
    EXPECT_EQ(values[3], -1);
}

int main(int argc, char **argv){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

}