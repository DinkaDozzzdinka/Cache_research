#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include <unistd.h>

#include "config.hpp"

namespace cache_tests
{
    class TemporaryConfig
    {

    TemporaryConfig(const TemporaryConfig&) = delete;
    TemporaryConfig& operator=(const TemporaryConfig&) = delete;
    
    public:
        TemporaryConfig()
        {
            char path_template[] = "/tmp/cache_config_test_XXXXXX";

            const int descriptor = ::mkstemp(path_template);

            if (descriptor == -1)
                throw std::runtime_error("Cannot create temporary config file");
            
            filepath_ = path_template;

            if (::close(descriptor) == -1)
            {
                std::error_code error;
                std::filesystem::remove(filepath_, error);

                throw std::runtime_error("Cannot close temporary config file");
            }
        }

        ~TemporaryConfig()
        {
            std::error_code error;
            std::filesystem::remove(filepath_, error);
        }

        bool Write(const std::string& contents) const
        {
            std::ofstream file(filepath_);

            if (!file)
                return false;

            file << contents;
            file.close();

            return !file.fail();
        }

        const std::string& Path() const
        {
            return filepath_;
        }

    private:
        std::string filepath_;
    };


    // missing or empty file ---------------------------------------
    TEST(ConfigTest, RejectsEmptyFile)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write(""));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsMissingFile)
    {
        TemporaryConfig file;
        ASSERT_TRUE(std::filesystem::remove(file.Path()));
        
        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, AcceptsWhitespaceAndLineBreaks)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("\n\t 2 \n LRU\t\nARC \n\n"));

        const auto config = caches::ReadConfig(file.Path());

        ASSERT_EQ(config.policies.size(), 2u);
        EXPECT_EQ(config.policies[0], caches::ParsePolicy("LRU"));
        EXPECT_EQ(config.policies[1], caches::ParsePolicy("ARC"));
    }

    TEST(ConfigTest, RejectsWhitespaceOnly)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write(" \n\t\n "));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    
    // invalid level tokens --------------------------------------------
    TEST(ConfigTest, RejectsZeroLevels)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("0"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsNegativeLevels)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("-2 LRU ARC"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsInvalidLevelTokens)
    {
        TemporaryConfig file;
        const std::string invalid_tokens[] = {
            "abc",
            "2.5",
            "2LRU",
            "2e1"
        };

        for (const auto& token : invalid_tokens)
        {
            SCOPED_TRACE(::testing::Message() << "token=" << token);
            ASSERT_TRUE(file.Write(token + " LRU ARC"));

            EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
        }
    }

    TEST(ConfigTest, RejectsLevelsAboveLimit)
    {
        TemporaryConfig file;
        std::string contents = "65\n";

        for (int i = 0; i < 65; ++i)
            contents += "LRU\n";
        ASSERT_TRUE(file.Write(contents));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsIntegerOverflow)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write(std::string(100, '9') + "\nLRU"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, ReadsSingleLevel)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("1 LRU"));

        const auto config = caches::ReadConfig(file.Path());

        ASSERT_EQ(config.policies.size(), 1u);
        EXPECT_EQ(config.policies[0], caches::ParsePolicy("LRU"));
    }


    // incorrect policies ------------------------------------------------------
    TEST(ConfigTest, PreservesPolicyOrder)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("3 LRU ARC LRU"));

        const auto config = caches::ReadConfig(file.Path());

        ASSERT_EQ(config.policies.size(), 3u);
        EXPECT_EQ(config.policies[0], caches::ParsePolicy("LRU"));
        EXPECT_EQ(config.policies[1], caches::ParsePolicy("ARC"));
        EXPECT_EQ(config.policies[2], caches::ParsePolicy("LRU"));
    }

    TEST(ConfigTest, RejectsExtraPolicy)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("2 LRU ARC LRU"));
        
        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsUnknownPolicy)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("2 LRU UNKNOWN"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::invalid_argument);
    }

    TEST(ConfigTest, RejectsMissingPolicies)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("3 LRU ARC"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }

    TEST(ConfigTest, RejectsGarbageAfterPolicies)
    {
        TemporaryConfig file;
        ASSERT_TRUE(file.Write("2 LRU ARC\n123"));

        EXPECT_THROW(caches::ReadConfig(file.Path()), std::runtime_error);
    }
} 