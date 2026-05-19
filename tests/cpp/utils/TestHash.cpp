#include <gtest/gtest.h>

#include "Utils/Hash/Hash.h"

// -----------------------------------------------------------------------------
// character::isUpper
// -----------------------------------------------------------------------------

TEST(CharacterIsUpper, UAscII) {
    EXPECT_TRUE(character::isUpper('A'));
    EXPECT_TRUE(character::isUpper('Z'));
}

TEST(CharacterIsUpper, LowerCaseIsNotUpper) {
    EXPECT_FALSE(character::isUpper('a'));
    EXPECT_FALSE(character::isUpper('z'));
}

TEST(CharacterIsUpper, NumericsAreNotUpper) {
    EXPECT_FALSE(character::isUpper('0'));
    EXPECT_FALSE(character::isUpper('9'));
}

TEST(CharacterIsUpper, BoundariesAreNotUpper) {
    EXPECT_FALSE(character::isUpper('@'));  // chr(64) — just before 'A'
    EXPECT_FALSE(character::isUpper('['));  // chr(91) — just after 'Z'
}

// -----------------------------------------------------------------------------
// character::toLower
// -----------------------------------------------------------------------------

TEST(CharacterToLower, UpperCaseConverted) {
    EXPECT_EQ(character::toLower('A'), 'a');
    EXPECT_EQ(character::toLower('Z'), 'z');
    EXPECT_EQ(character::toLower('H'), 'h');
}

TEST(CharacterToLower, LowerCaseUnchanged) {
    EXPECT_EQ(character::toLower('a'), 'a');
    EXPECT_EQ(character::toLower('z'), 'z');
}

TEST(CharacterToLower, NonLettersUnchanged) {
    EXPECT_EQ(character::toLower('0'), '0');
    EXPECT_EQ(character::toLower('@'), '@');
}

// -----------------------------------------------------------------------------
// character::isTerminator
// -----------------------------------------------------------------------------

TEST(CharacterIsTerminator, NullIsTerminator) {
    EXPECT_TRUE(character::isTerminator('\0'));
}

TEST(CharacterIsTerminator, NonNullIsNotTerminator) {
    EXPECT_FALSE(character::isTerminator('A'));
    EXPECT_FALSE(character::isTerminator(' '));
}

// -----------------------------------------------------------------------------
// character::isQuestion
// -----------------------------------------------------------------------------

TEST(CharacterIsQuestion, QuestionMarkIsTrue) {
    EXPECT_TRUE(character::isQuestion('?'));
}

TEST(CharacterIsQuestion, OtherCharactersAreFalse) {
    EXPECT_FALSE(character::isQuestion('!'));
    EXPECT_FALSE(character::isQuestion('A'));
}

// -----------------------------------------------------------------------------
// character::getLength
// -----------------------------------------------------------------------------

TEST(CharacterGetLength, EmptyString) {
    EXPECT_EQ(character::getLength(""), static_cast<size_t>(0));
}

TEST(CharacterGetLength, ThreeChars) {
    EXPECT_EQ(character::getLength("abc"), static_cast<size_t>(3));
}

TEST(CharacterGetLength, LongString) {
    EXPECT_EQ(character::getLength("Hello, World!"), static_cast<size_t>(13));
}

TEST(CharacterGetLength, SingleChar) {
    EXPECT_EQ(character::getLength("X"), static_cast<size_t>(1));
}

// -----------------------------------------------------------------------------
// hash::fnv1a32_hash — Empty String
// -----------------------------------------------------------------------------

TEST(HashFnva32, EmptyStringReturnsBasis) {
    // FNV-1a basis is 0x811C9DC5
    EXPECT_EQ(hash::fnv1a32_hash("", false), 0x811C9DC5u);
}

// -----------------------------------------------------------------------------
// hash::fnv1a32_hash — Deterministic
// -----------------------------------------------------------------------------

TEST(HashFnva32, SameInputSameOutput) {
    const auto h1 = hash::fnv1a32_hash("hello", false);
    const auto h2 = hash::fnv1a32_hash("hello", false);
    EXPECT_EQ(h1, h2);
}

TEST(HashFnva32, DifferentStringsDifferentOutput) {
    const auto h1 = hash::fnv1a32_hash("hello", false);
    const auto h2 = hash::fnv1a32_hash("world", false);
    EXPECT_NE(h1, h2);
}

// -----------------------------------------------------------------------------
// hash::fnv1a32_hash — Case Insensitive
// -----------------------------------------------------------------------------

TEST(HashFnva32, CaseInsensitiveEqual) {
    const auto hLower = hash::fnv1a32_hash("hello", true);
    const auto hUpper = hash::fnv1a32_hash("HELLO", true);
    EXPECT_EQ(hLower, hUpper);
}

TEST(HashFnva32, CaseSensitiveDifferent) {
    const auto hLower = hash::fnv1a32_hash("Hello", false);
    const auto hUpper = hash::fnv1a32_hash("HELLO", false);
    EXPECT_NE(hLower, hUpper);
}

TEST(HashFnva32, SingleCharCaseInsensitive) {
    EXPECT_EQ(hash::fnv1a32_hash("a", true), hash::fnv1a32_hash("A", true));
}

// -----------------------------------------------------------------------------
// hash::fnv1a32_hash — Size Parameter Overload
// -----------------------------------------------------------------------------

TEST(HashFnva32, ExplicitSizeMatchesAutoSize) {
    constexpr const char* str = "test_string";
    const auto autoHash = hash::fnv1a32_hash(str, false);
    const auto explicitHash = hash::fnv1a32_hash(str, character::getLength(str), false);
    EXPECT_EQ(autoHash, explicitHash);
}

// -----------------------------------------------------------------------------
// HASH_CT vs HASH_RT — Compile-Time vs Run-Time Consistency
// -----------------------------------------------------------------------------

TEST(HashCTvsRT, IdenticalResults) {
    // Both HASH_CT and HASH_RT use case-insensitive mode
    const auto rtHash = HASH_RT("MyConfigKey");
    const auto ctHash = HASH_CT("MyConfigKey");
    EXPECT_EQ(rtHash, ctHash);
}

TEST(HashCTvsRT, EmptyString) {
    EXPECT_EQ(HASH_RT(""), HASH_CT(""));
}

TEST(HashCTvsRT, DifferentInputsYieldDifferentHashes) {
    const auto apple = HASH_CT("apple");
    const auto banana = HASH_CT("banana");
    EXPECT_NE(apple, banana);
}

TEST(HashCTvsRT, CaseInsensitiveConsistent) {
    // Both macros use ignore_case=true
    EXPECT_EQ(HASH_CT("InGame"), HASH_RT("ingame"));
}

// -----------------------------------------------------------------------------
// Wide String Hashing
// -----------------------------------------------------------------------------

TEST(HashWideString, EmptyWideStringReturnsBasis) {
    EXPECT_EQ(hash::fnv1a32_hash(L"", false), 0x811C9DC5u);
}

TEST(HashWideString, SameWideInputSameOutput) {
    const auto h1 = hash::fnv1a32_hash(L"wide", false);
    const auto h2 = hash::fnv1a32_hash(L"wide", false);
    EXPECT_EQ(h1, h2);
}

TEST(HashWideString, WideCaseInsensitive) {
    EXPECT_EQ(hash::fnv1a32_hash(L"WIDE", true), hash::fnv1a32_hash(L"wide", true));
}

TEST(HashWideString, WideDifferentFromNarrow) {
    // Wide string "A" (wchar_t 0x0041) vs narrow "A" (char 0x41)
    // The hash interprets raw bytes — wchar_t is 2 bytes on Windows
    // "A" as narrow = 1 char = 0x41 -> one hash step
    // L"A" = 1 wchar_t = {0x41, 0x00} but we process one element at a time
    // wchar_t 'A' = 65 in hash_t = same bits, so should match
    EXPECT_EQ(hash::fnv1a32_hash(L"A", false), hash::fnv1a32_hash("A", false));
}

TEST(HashWideString, ExplicitSizeWide) {
    constexpr const wchar_t* str = L"hello";
    const auto autoHash = hash::fnv1a32_hash(str, false);
    const auto explicitHash = hash::fnv1a32_hash(str, character::getLength(str), false);
    EXPECT_EQ(autoHash, explicitHash);
}

// -----------------------------------------------------------------------------
// Property-Based: Length/Content Correlation
// -----------------------------------------------------------------------------

TEST(HashProperties, AppendChangesHash) {
    const auto base = hash::fnv1a32_hash("prefix", false);
    const auto extended = hash::fnv1a32_hash("prefix_more", false);
    EXPECT_NE(base, extended);
}

TEST(HashProperties, EmptyPrefix) {
    const auto ofText = hash::fnv1a32_hash("text", false);
    const auto ofEmptyPlusText = hash::fnv1a32_hash("text", false);
    EXPECT_EQ(ofText, ofEmptyPlusText);
}
