//  Copyright 2022, University of Freiburg,
//  Chair of Algorithms and Data Structures.
//  Author: Johannes Kalmbach <kalmbach@cs.uni-freiburg.de>

#include <absl/hash/hash_testing.h>
#include <gtest/gtest.h>

#include <bitset>

#include "./ValueIdTestHelpers.h"
#include "./util/GTestHelpers.h"
#include "./util/IndexTestHelpers.h"
#include "global/ValueId.h"
#include "util/HashSet.h"
#include "util/Random.h"
#include "util/Serializer/ByteBufferSerializer.h"
#include "util/Serializer/Serializer.h"

struct ValueIdTest : public ::testing::Test {
  ValueIdTest() {
    // We need to initialize a (static). index, otherwise we can't compare
    // VocabIndex to LocalVocabIndex entries
    ad_utility::testing::getQec();
  }
};

TEST_F(ValueIdTest, makeFromDouble) {
  auto testRepresentableDouble = [](double d) {
    auto id = ValueId::makeFromDouble(d);
    ASSERT_EQ(id.getDatatype(), Datatype::Double);
    // We lose `numDatatypeBits` bits of precision, so `ASSERT_DOUBLE_EQ` would
    // fail.
    ASSERT_FLOAT_EQ(id.getDouble(), d);
    // This check expresses the precision more exactly
    if (id.getDouble() != d) {
      // The if is needed for the case of += infinity.
      ASSERT_NEAR(id.getDouble(), d,
                  std::abs(d / (1ul << (52 - ValueId::numDatatypeBits))));
    }
  };

  auto testNonRepresentableSubnormal = [](double d) {
    auto id = ValueId::makeFromDouble(d);
    ASSERT_EQ(id.getDatatype(), Datatype::Double);
    // Subnormal numbers with a too small fraction are rounded to zero.
    ASSERT_EQ(id.getDouble(), 0.0);
  };
  for (size_t i = 0; i < 10'000; ++i) {
    testRepresentableDouble(positiveRepresentableDoubleGenerator());
    testRepresentableDouble(negativeRepresentableDoubleGenerator());
    auto nonRepresentable = nonRepresentableDoubleGenerator();
    // The random number generator includes the edge cases which would make the
    // tests fail.
    if (nonRepresentable != ValueId::minPositiveDouble &&
        nonRepresentable != -ValueId::minPositiveDouble) {
      testNonRepresentableSubnormal(nonRepresentable);
    }
  }

  testRepresentableDouble(std::numeric_limits<double>::infinity());
  testRepresentableDouble(-std::numeric_limits<double>::infinity());

  // Test positive and negative 0.
  ASSERT_NE(absl::bit_cast<uint64_t>(0.0), absl::bit_cast<uint64_t>(-0.0));
  ASSERT_EQ(0.0, -0.0);
  testRepresentableDouble(0.0);
  testRepresentableDouble(-0.0);
  testNonRepresentableSubnormal(0.0);
  testNonRepresentableSubnormal(0.0);

  auto quietNan = std::numeric_limits<double>::quiet_NaN();
  auto signalingNan = std::numeric_limits<double>::signaling_NaN();
  ASSERT_TRUE(std::isnan(ValueId::makeFromDouble(quietNan).getDouble()));
  ASSERT_TRUE(std::isnan(ValueId::makeFromDouble(signalingNan).getDouble()));

  // Test that the value of `minPositiveDouble` is correct.
  auto testSmallestNumber = [](double d) {
    ASSERT_EQ(ValueId::makeFromDouble(d).getDouble(), d);
    ASSERT_NE(d / 2, 0.0);
    ASSERT_EQ(ValueId::makeFromDouble(d / 2).getDouble(), 0.0);
  };
  testSmallestNumber(ValueId::minPositiveDouble);
  testSmallestNumber(-ValueId::minPositiveDouble);
}

TEST_F(ValueIdTest, makeFromInt) {
  for (size_t i = 0; i < 10'000; ++i) {
    auto value = nonOverflowingNBitGenerator();
    auto id = ValueId::makeFromInt(value);
    ASSERT_EQ(id.getDatatype(), Datatype::Int);
    ASSERT_EQ(id.getInt(), value);
  }

  auto testOverflow = [](auto generator) {
    using I = ValueId::IntegerType;
    for (size_t i = 0; i < 10'000; ++i) {
      auto value = generator();
      auto id = ValueId::makeFromInt(value);
      ASSERT_EQ(id.getDatatype(), Datatype::Int);
      ASSERT_EQ(id.getInt(), I::fromNBit(I::toNBit(value)));
      ASSERT_NE(id.getInt(), value);
    }
  };

  testOverflow(overflowingNBitGenerator);
  testOverflow(underflowingNBitGenerator);
}

// _____________________________________________________________________________
TEST_F(ValueIdTest, makeFromBool) {
  EXPECT_TRUE(ValueId::makeBoolFromZeroOrOne(true).getBool());
  EXPECT_TRUE(ValueId::makeFromBool(true).getBool());
  EXPECT_FALSE(ValueId::makeBoolFromZeroOrOne(false).getBool());
  EXPECT_FALSE(ValueId::makeFromBool(false).getBool());

  EXPECT_EQ(ValueId::makeBoolFromZeroOrOne(true).getBoolLiteral(), "1");
  EXPECT_EQ(ValueId::makeFromBool(true).getBoolLiteral(), "true");
  EXPECT_EQ(ValueId::makeBoolFromZeroOrOne(false).getBoolLiteral(), "0");
  EXPECT_EQ(ValueId::makeFromBool(false).getBoolLiteral(), "false");
}

TEST_F(ValueIdTest, Indices) {
  auto testRandomIds = [&](auto makeId, auto getFromId, Datatype type) {
    auto testSingle = [&](auto value) {
      auto id = makeId(value);
      ASSERT_EQ(id.getDatatype(), type);
      ASSERT_EQ(std::invoke(getFromId, id), value);
    };
    for (size_t idx = 0; idx < 10'000; ++idx) {
      testSingle(indexGenerator());
    }
    testSingle(0);
    testSingle(ValueId::maxIndex);

    if (type != Datatype::LocalVocabIndex) {
      for (size_t idx = 0; idx < 10'000; ++idx) {
        auto value = invalidIndexGenerator();
        ASSERT_THROW(makeId(value), ValueId::IndexTooLargeException);
        AD_EXPECT_THROW_WITH_MESSAGE(
            makeId(value), ::testing::ContainsRegex("is bigger than"));
      }
    }
  };

  testRandomIds(&makeTextRecordId, &getTextRecordIndex,
                Datatype::TextRecordIndex);
  testRandomIds(&makeVocabId, &getVocabIndex, Datatype::VocabIndex);

  auto localVocabWordToInt = [](const auto& input) {
    return std::atoll(getLocalVocabIndex(input).c_str());
  };
  testRandomIds(&makeLocalVocabId, localVocabWordToInt,
                Datatype::LocalVocabIndex);
  testRandomIds(&makeWordVocabId, &getWordVocabIndex, Datatype::WordVocabIndex);
}

TEST_F(ValueIdTest, Undefined) {
  auto id = ValueId::makeUndefined();
  ASSERT_EQ(id.getDatatype(), Datatype::Undefined);
}

TEST_F(ValueIdTest, OrderingDifferentDatatypes) {
  auto ids = makeRandomIds();
  std::sort(ids.begin(), ids.end());

  auto compareByDatatypeAndIndexTypes = [](ValueId a, ValueId b) {
    auto typeA = a.getDatatype();
    auto typeB = b.getDatatype();
    if (ad_utility::contains(ValueId::stringTypes_, typeA) &&
        ad_utility::contains(ValueId::stringTypes_, typeB)) {
      return false;
    }
    return a.getDatatype() < b.getDatatype();
  };
  ASSERT_TRUE(
      std::is_sorted(ids.begin(), ids.end(), compareByDatatypeAndIndexTypes));
}

TEST_F(ValueIdTest, IndexOrdering) {
  auto testOrder = [](auto makeIdFromIndex, auto getIndexFromId) {
    std::vector<ValueId> ids;
    addIdsFromGenerator(indexGenerator, makeIdFromIndex, ids);
    std::vector<std::invoke_result_t<decltype(getIndexFromId), ValueId>>
        indices;
    for (auto id : ids) {
      indices.push_back(std::invoke(getIndexFromId, id));
    }

    std::sort(ids.begin(), ids.end());
    std::sort(indices.begin(), indices.end());

    for (size_t i = 0; i < ids.size(); ++i) {
      ASSERT_EQ(std::invoke(getIndexFromId, ids[i]), indices[i]);
    }
  };

  testOrder(&makeVocabId, &getVocabIndex);
  testOrder(&makeLocalVocabId, &getLocalVocabIndex);
  testOrder(&makeWordVocabId, &getWordVocabIndex);
  testOrder(&makeTextRecordId, &getTextRecordIndex);
}

TEST_F(ValueIdTest, DoubleOrdering) {
  auto ids = makeRandomDoubleIds();
  std::vector<double> doubles;
  doubles.reserve(ids.size());
  for (auto id : ids) {
    doubles.push_back(id.getDouble());
  }
  std::sort(ids.begin(), ids.end());

  // The sorting of `double`s is broken as soon as NaNs are present. We remove
  // the NaNs from the `double`s.
  std::erase_if(doubles, [](double d) { return std::isnan(d); });
  std::sort(doubles.begin(), doubles.end());

  // When sorting ValueIds that hold doubles, the NaN values form a contiguous
  // range.
  auto beginOfNans = std::find_if(ids.begin(), ids.end(), [](const auto& id) {
    return std::isnan(id.getDouble());
  });
  auto endOfNans = std::find_if(ids.rbegin(), ids.rend(), [](const auto& id) {
                     return std::isnan(id.getDouble());
                   }).base();
  for (auto it = beginOfNans; it < endOfNans; ++it) {
    ASSERT_TRUE(std::isnan(it->getDouble()));
  }

  // The NaN values are sorted directly after positive infinity.
  ASSERT_EQ((beginOfNans - 1)->getDouble(),
            std::numeric_limits<double>::infinity());
  // Delete the NaN values without changing the order of all other types.
  ids.erase(beginOfNans, endOfNans);

  // In `ids` the negative number stand AFTER the positive numbers because of
  // the bitOrdering. First rotate the negative numbers to the beginning.
  auto doubleIdIsNegative = [](ValueId id) {
    auto bits = absl::bit_cast<uint64_t>(id.getDouble());
    return bits & ad_utility::bitMaskForHigherBits(1);
  };
  auto beginOfNegatives =
      std::find_if(ids.begin(), ids.end(), doubleIdIsNegative);
  auto endOfNegatives = std::rotate(ids.begin(), beginOfNegatives, ids.end());

  // The negative numbers now come before the positive numbers, but the are
  // ordered in descending instead of ascending order, reverse them.
  std::reverse(ids.begin(), endOfNegatives);

  // After these two transformations (switch positive and negative range,
  // reverse negative range) the `ids` are sorted in exactly the same order as
  // the `doubles`.
  for (size_t i = 0; i < ids.size(); ++i) {
    auto doubleTruncated = ValueId::makeFromDouble(doubles[i]).getDouble();
    ASSERT_EQ(ids[i].getDouble(), doubleTruncated);
  }
}

TEST_F(ValueIdTest, SignedIntegerOrdering) {
  std::vector<ValueId> ids;
  addIdsFromGenerator(nonOverflowingNBitGenerator, &ValueId::makeFromInt, ids);
  std::vector<int64_t> integers;
  integers.reserve(ids.size());
  for (auto id : ids) {
    integers.push_back(id.getInt());
  }

  std::sort(ids.begin(), ids.end());
  std::sort(integers.begin(), integers.end());

  // The negative integers stand after the positive integers, so we have to
  // switch these ranges.
  auto beginOfNegative = std::find_if(
      ids.begin(), ids.end(), [](ValueId id) { return id.getInt() < 0; });
  std::rotate(ids.begin(), beginOfNegative, ids.end());

  // Now `integers` and `ids` should be in the same order
  for (size_t i = 0; i < ids.size(); ++i) {
    ASSERT_EQ(ids[i].getInt(), integers[i]);
  }
}

TEST_F(ValueIdTest, Serialization) {
  auto ids = makeRandomIds();

  for (auto id : ids) {
    ad_utility::serialization::ByteBufferWriteSerializer writer;
    writer << id;
    ad_utility::serialization::ByteBufferReadSerializer reader{
        std::move(writer).data()};
    ValueId serializedId;
    reader >> serializedId;
    ASSERT_EQ(id, serializedId);
  }
}

TEST_F(ValueIdTest, Hashing) {
  {
    auto ids = makeRandomIds();
    ad_utility::HashSet<ValueId> idsWithoutDuplicates;
    for (size_t i = 0; i < 2; ++i) {
      for (auto id : ids) {
        idsWithoutDuplicates.insert(id);
      }
    }
    std::vector<ValueId> idsWithoutDuplicatesAsVector(
        idsWithoutDuplicates.begin(), idsWithoutDuplicates.end());

    std::sort(idsWithoutDuplicatesAsVector.begin(),
              idsWithoutDuplicatesAsVector.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    ASSERT_EQ(ids, idsWithoutDuplicatesAsVector);
  }
  {
    using namespace ad_utility::triple_component;
    using namespace ad_utility::testing;
    const Index& index = getQec()->getIndex();
    auto mkId = makeGetId(index);
    LocalVocab lv1;
    LocalVocab lv2;
    Iri iri = Iri::fromIriref("<foo>");
    LocalVocabEntry lve1(iri);
    LocalVocabEntry lve2(iri);
    LocalVocabEntry lve3(Literal::fromStringRepresentation("\"foo\""));
    LocalVocabEntry lve4(Iri::fromIriref("<x>"));
    auto LVID = [](LocalVocabEntry& lve, LocalVocab& lv) {
      return Id::makeFromLocalVocabIndex(lv.getIndexAndAddIfNotContained(lve));
    };
    // Checks that hashing is implemented correctly using `==` for equality. The
    // hash expansion is the values added with `combine`.
    // - If two elements are equal, then their hash expansions must be equal.
    // - If two elements are not equal, then hash expansions must differ and
    // neither can be a suffix of the other.
    EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
        {LVID(lve1, lv1), LVID(lve2, lv2), LVID(lve3, lv1), LVID(lve4, lv1),
         mkId("<x>"), Id::makeFromInt(0), Id::makeFromInt(42),
         Id::makeFromDouble(0), Id::makeFromDouble(1.56),
         Id::makeFromDouble(1e-10), Id::makeFromDouble(1e+100),
         Id::makeFromBool(true), Id::makeFromBool(false),
         Id::makeUndefined()}));
  }
}

TEST_F(ValueIdTest, toDebugString) {
  auto test = [](ValueId id, std::string_view expected) {
    std::stringstream stream;
    stream << id;
    ASSERT_EQ(stream.str(), expected);
  };
  test(ValueId::makeUndefined(), "U:0");
  // Values with type undefined can usually only have one value (all data bits
  // zero). Sometimes ValueIds with type undefined but non-zero data bits are
  // used. The following test tests one of these internal ValueIds.
  ValueId customUndefined = ValueId::fromBits(
      ValueId::IntegerType::fromNBit(100) |
      (static_cast<ValueId::T>(Datatype::Undefined) << ValueId::numDataBits));
  test(customUndefined, "U:100");
  test(ValueId::makeFromDouble(42.0), "D:42.000000");
  test(ValueId::makeFromBool(false), "B:false");
  test(ValueId::makeFromBool(true), "B:true");
  test(ValueId::makeBoolFromZeroOrOne(false), "B:false");
  test(ValueId::makeBoolFromZeroOrOne(true), "B:true");
  test(makeVocabId(15), "V:15");
  auto str = LocalVocabEntry{
      ad_utility::triple_component::LiteralOrIri::literalWithoutQuotes(
          "SomeValue")};
  test(ValueId::makeFromLocalVocabIndex(&str), "L:\"SomeValue\"");
  test(makeTextRecordId(37), "T:37");
  test(makeWordVocabId(42), "W:42");
  test(makeBlankNodeId(27), "B:27");
  test(ValueId::makeFromDate(
           DateYearOrDuration{123456, DateYearOrDuration::Type::Year}),
       "D:123456");
  test(ValueId::makeFromGeoPoint(GeoPoint{50.0, 50.0}),
       "G:POINT(50.000000 50.000000)");
  // make an ID with an invalid datatype
  ASSERT_ANY_THROW(test(ValueId::max(), "blim"));
}

TEST_F(ValueIdTest, InvalidDatatypeEnumValue) {
  ASSERT_ANY_THROW(toString(static_cast<Datatype>(2345)));
}

TEST_F(ValueIdTest, TriviallyCopyable) {
  static_assert(std::is_trivially_copyable_v<ValueId>);
}
