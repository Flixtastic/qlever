// Copyright 2025, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Felix Meisen (fesemeisen@outlook.de)

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../util/GTestHelpers.h"
#include "../util/IdTableHelpers.h"
#include "../util/IndexTestHelpers.h"
#include "./TextIndexScanTestHelpers.h"
#include "engine/TextIndexScanForWord.h"

using namespace ad_utility::testing;
using ad_utility::source_location;
namespace h = textIndexScanTestHelpers;

namespace {

std::string kg =
    "<a> <p> \"he failed the test\" . <a> <p> \"testing can help\" . <a> <p> "
    "\"some other sentence\" .";

// Sorted order is:
// "\"he failed the test\""
// "\"some other sentence\""
// "\"testing can help\""

auto getQecWithTextIndex(size_t nofWordPostingsPerTextBlock) {
  using namespace ad_utility::testing;
  TestIndexConfig config{kg};
  config.createTextIndex = true;
  config.textBlockSize = nofWordPostingsPerTextBlock;
  return getQec(std::move(config));
}

TEST(TextIndexScan, TestTextBlockSizes) {
  AD_EXPECT_THROW_WITH_MESSAGE(
      getQecWithTextIndex(0),
      ::testing::HasSubstr(
          "Number of word postings in text block has to be larger than zero."));
  for (size_t index = 1; index <= 11; ++index) {
    auto qec = getQecWithTextIndex(index);
    TextIndexScanForWord s1{qec, Variable{"?text"}, "test*"};
    ASSERT_EQ(s1.getResultWidth(), 3);
    auto result1 = s1.computeResultOnlyForTesting();
    auto tr1 = h::TextResult{qec, result1, true};
    ASSERT_EQ(result1.idTable().numColumns(), 3);
    ASSERT_EQ(result1.idTable().size(), 2);
    ASSERT_EQ(h::combineToString("\"he failed the test\"", "test"),
              tr1.getRow(0));
    ASSERT_EQ(h::combineToString("\"testing can help\"", "testing"),
              tr1.getRow(1));

    TextIndexScanForWord s2{qec, Variable{"?text"}, "he"};
    ASSERT_EQ(s2.getResultWidth(), 2);
    auto result2 = s2.computeResultOnlyForTesting();
    auto tr2 = h::TextResult{qec, result2, false};
    ASSERT_EQ(result2.idTable().numColumns(), 2);
    ASSERT_EQ(result2.idTable().size(), 1);
    ASSERT_EQ("\"he failed the test\"", tr2.getTextRecord(0));

    TextIndexScanForWord s3{qec, Variable{"?text"}, "*"};
    ASSERT_EQ(s3.getResultWidth(), 3);
    auto result3 = s3.computeResultOnlyForTesting();
    auto tr3 = h::TextResult{qec, result3, true};
    ASSERT_EQ(result3.idTable().numColumns(), 3);
    ASSERT_EQ(result3.idTable().size(), 10);
    tr3.checkUnorderedListOfWordsInRange({"he", "failed", "the", "test"}, 0, 4);
    tr3.checkUnorderedListOfWordsInRange({"some", "other", "sentence"}, 4, 7);
    tr3.checkUnorderedListOfWordsInRange({"testing", "can", "help"}, 7, 10);
    ASSERT_EQ("\"he failed the test\"", tr3.getTextRecord(0));
    ASSERT_EQ("\"he failed the test\"", tr3.getTextRecord(1));
    ASSERT_EQ("\"he failed the test\"", tr3.getTextRecord(2));
    ASSERT_EQ("\"he failed the test\"", tr3.getTextRecord(3));
    ASSERT_EQ("\"some other sentence\"", tr3.getTextRecord(4));
    ASSERT_EQ("\"some other sentence\"", tr3.getTextRecord(5));
    ASSERT_EQ("\"some other sentence\"", tr3.getTextRecord(6));
    ASSERT_EQ("\"testing can help\"", tr3.getTextRecord(7));
    ASSERT_EQ("\"testing can help\"", tr3.getTextRecord(8));
    ASSERT_EQ("\"testing can help\"", tr3.getTextRecord(9));
  }
}

}  // namespace
