/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include <iostream>
#include <fstream>
#include <gtest/gtest.h>

#include "paddle/fluid/train/custom_trainer/feed/executor/executor.h"
#include "paddle/fluid/framework/tensor_util.h"
#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/framework/io/fs.h"
#include "paddle/fluid/string/string_helper.h"
#include "paddle/fluid/train/custom_trainer/feed/dataset/data_reader.h"

namespace paddle {
namespace custom_trainer {
namespace feed {

namespace {
const char test_data_dir[] = "test_data";
}

class DataReaderTest : public testing::Test
{
public:
    static void SetUpTestCase()
    {
        ::paddle::framework::localfs_mkdir(test_data_dir);

        {
            std::ofstream fout(::paddle::framework::fs_path_join(test_data_dir, "a.txt"));
            fout << "abc 123456" << std::endl;
            fout << "def 234567" << std::endl;
            fout.close();
        }

        {
            std::ofstream fout(::paddle::framework::fs_path_join(test_data_dir, "b.txt"));
            fout << "ghi 345678" << std::endl;
            fout << "jkl 456789" << std::endl;
            fout.close();
        }
    }

    static void TearDownTestCase()
    {
        ::paddle::framework::localfs_remove(test_data_dir);
    }

    virtual void SetUp()
    {
        context_ptr.reset(new TrainerContext());
    }

    virtual void TearDown()
    {
        context_ptr = nullptr;
    }

    std::shared_ptr<TrainerContext> context_ptr;
};

TEST_F(DataReaderTest, LineDataParser) {
    std::unique_ptr<DataParser> data_parser(CREATE_CLASS(DataParser, "LineDataParser"));
    
    ASSERT_NE(nullptr, data_parser);
    auto config = YAML::Load("");

    ASSERT_EQ(0, data_parser->initialize(config, context_ptr));

    DataItem data_item;
    ASSERT_NE(0, data_parser->parse(std::string("1abcd123456"), data_item));
    ASSERT_EQ(0, data_parser->parse(std::string("2abc 123456"), data_item));
    ASSERT_STREQ("2abc", data_item.id.c_str());
    ASSERT_STREQ("123456", data_item.data.c_str());

    ASSERT_NE(0, data_parser->parse("3abcd123456", data_item));
    ASSERT_EQ(0, data_parser->parse("4abc 123456", data_item));
    ASSERT_STREQ("4abc", data_item.id.c_str());
    ASSERT_STREQ("123456", data_item.data.c_str());

    ASSERT_NE(0, data_parser->parse("5abc 123456", 4, data_item));
    ASSERT_EQ(0, data_parser->parse("6abc 123456", 5, data_item));
    ASSERT_STREQ("6abc", data_item.id.c_str());
    ASSERT_STREQ("", data_item.data.c_str());

    ASSERT_EQ(0, data_parser->parse("7abc 123456", 8, data_item));
    ASSERT_STREQ("7abc", data_item.id.c_str());
    ASSERT_STREQ("123", data_item.data.c_str());
}

TEST_F(DataReaderTest, LineDataReader) {
    std::unique_ptr<DataReader> data_reader(CREATE_CLASS(DataReader, "LineDataReader"));
    ASSERT_NE(nullptr, data_reader);

    YAML::Node config = YAML::Load("parser:\n"
                                   "    class: LineDataParser\n"
                                   "pipeline_cmd: cat\n"
                                   "done_file: done_file");
    ASSERT_EQ(0, data_reader->initialize(config, context_ptr));

    config = YAML::Load("parser:\n"
                        "    class: LineDataParser\n"
                        "pipeline_cmd: cat\n"
                        "done_file: done_file\n"
                        "buffer_size: 128");
    ASSERT_EQ(0, data_reader->initialize(config, context_ptr));

    ASSERT_FALSE(data_reader->is_data_ready(test_data_dir));
    std::ofstream fout(::paddle::framework::fs_path_join(test_data_dir, "done_file"));
    fout << "done";
    fout.close();
    ASSERT_TRUE(data_reader->is_data_ready(test_data_dir));

    auto channel = ::paddle::framework::MakeChannel<DataItem>();
    ASSERT_NE(nullptr, channel);
    ASSERT_EQ(0, data_reader->read_all(test_data_dir, channel));

    framework::ChannelReader<DataItem> reader(channel.get());
    DataItem data_item;
    
    reader >> data_item;
    ASSERT_TRUE(reader);
    ASSERT_STREQ("abc", data_item.id.c_str());
    ASSERT_STREQ("123456", data_item.data.c_str());

    reader >> data_item;
    ASSERT_TRUE(reader);
    ASSERT_STREQ("def", data_item.id.c_str());
    ASSERT_STREQ("234567", data_item.data.c_str());

    reader >> data_item;
    ASSERT_TRUE(reader);
    ASSERT_STREQ("ghi", data_item.id.c_str());
    ASSERT_STREQ("345678", data_item.data.c_str());

    reader >> data_item;
    ASSERT_TRUE(reader);
    ASSERT_STREQ("jkl", data_item.id.c_str());
    ASSERT_STREQ("456789", data_item.data.c_str());

    reader >> data_item;
    ASSERT_FALSE(reader);
}

}  // namespace feed
}  // namespace custom_trainer
}  // namespace paddle
