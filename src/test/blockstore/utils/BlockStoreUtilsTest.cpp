#include "gtest/gtest.h"

#include "blockstore/implementations/testfake/FakeBlockStore.h"
#include "test/testutils/DataBlockFixture.h"
#include "blockstore/utils/BlockStoreUtils.h"

#include <memory>

using ::testing::Test;
using ::testing::WithParamInterface;
using ::testing::Values;

using std::make_unique;
using std::unique_ptr;

using namespace blockstore;
using namespace blockstore::utils;

using blockstore::testfake::FakeBlockStore;

class BlockStoreUtilsTest: public Test {
public:
  unsigned int SIZE = 1024 * 1024;
  BlockStoreUtilsTest():
    ZEROES(SIZE),
    dataFixture(SIZE),
    blockStore(make_unique<FakeBlockStore>()) {
    ZEROES.FillWithZeroes();
  }

  Data ZEROES;
  DataBlockFixture dataFixture;
  unique_ptr<BlockStore> blockStore;
};

TEST_F(BlockStoreUtilsTest, CopyEmptyBlock) {
  auto block = blockStore->create(0);
  auto block2 = copyToNewBlock(blockStore.get(), *block.block);

  EXPECT_EQ(0u, block2.block->size());
}

TEST_F(BlockStoreUtilsTest, CopyZeroBlock) {
  auto block = blockStore->create(SIZE).block;
  auto block2 = copyToNewBlock(blockStore.get(), *block).block;

  EXPECT_EQ(SIZE, block2->size());
  EXPECT_EQ(0, std::memcmp(ZEROES.data(), block2->data(), SIZE));
}

TEST_F(BlockStoreUtilsTest, CopyDataBlock) {
  auto block = blockStore->create(SIZE).block;
  std::memcpy(block->data(), dataFixture.data(), SIZE);
  auto block2 = copyToNewBlock(blockStore.get(), *block).block;

  EXPECT_EQ(SIZE, block2->size());
  EXPECT_EQ(0, std::memcmp(dataFixture.data(), block2->data(), SIZE));
}

TEST_F(BlockStoreUtilsTest, OriginalBlockUnchanged) {
  auto block = blockStore->create(SIZE).block;
  std::memcpy(block->data(), dataFixture.data(), SIZE);
  auto block2 = copyToNewBlock(blockStore.get(), *block).block;

  EXPECT_EQ(SIZE, block->size());
  EXPECT_EQ(0, std::memcmp(dataFixture.data(), block->data(), SIZE));
}