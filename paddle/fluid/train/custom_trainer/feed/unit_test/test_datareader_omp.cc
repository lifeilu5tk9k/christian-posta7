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
#include <algorithm>
#include <gtest/gtest.h>
#include <omp.h>

#include "paddle/fluid/train/custom_trainer/feed/trainer_context.h"
#include "paddle/fluid/train/custom_trainer/feed/executor/executor.h"
#include "paddle/fluid/framework/tensor_util.h"
#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/train/custom_trainer/feed/io/file_system.h"
#include "paddle/fluid/train/custom_trainer/feed/io/shell.h"
#include "paddle/fluid/string/string_helper.h"
#include "paddle/fluid/train/custom_trainer/feed/dataset/data_reader.h"

namespace paddle {
namespace custom_trainer {
namespace feed {

namespace {
const char test_data_dir[] = "test_data";
}

class DataReaderOmpTest : public testing::Test {
public:
    static void SetUpTestCase() {
        std::unique_ptr<FileSystem> fs(CREATE_INSTANCE(FileSystem, "LocalFileSystem"));
        fs->mkdir(test_data_dir);
        shell_set_verbose(true);
        std_items.clear();
        sorted_std_items.clear();
        for (char c = 'a'; c <= 'z'; ++c) {
            DataItem item;
            item.id = c;
            item.data = std::to_string(c - 'a');
            std::ofstream fout(fs->path_join(test_data_dir, string::format_string("%c.txt", c)));
            fout << item.id << " " << item.data << std::endl;
            fout.close();
            sorted_std_items.push_back(std::move(item));
        }
        for (const auto& filename: fs->list(test_data_dir)) {
            std::ifstream fin(filename);
            DataItem item;
            fin >> item.id >> item.data;
            fin.close();
            std_items.push_back(std::move(item));
        }
    }

    static void TearDownTestCase() {
        std::unique_ptr<FileSystem> fs(CREATE_INSTANCE(FileSystem, "LocalFileSystem"));
        fs->remove(test_data_dir);
    }

    virtual void SetUp() {
        thread_num = omp_get_max_threads();
        omp_set_num_threads(1);
        fs.reset(CREATE_INSTANCE(FileSystem, "LocalFileSystem"));
        context_ptr.reset(new TrainerContext());
    }

    virtual void TearDown() {
        omp_set_num_threads(thread_num);
        fs = nullptr;
        context_ptr = nullptr;
    }

    static bool is_same(const std::vector<DataItem>& a, const std::vector<DataItem>& b) {
        int a_size = a.size();
        if (a_size != b.size()) {
            return false;
        }
        for (int i = 0; i < a_size; ++i) {
            if (a[i].id != b[i].id || a[i].data != b[i].data) {
                return false;
            }
        }
        return true;
    }

    static bool is_same_with_std_items(const std::vector<DataItem>& items) {
        return is_same(items, std_items);
    }

    static bool is_same_with_sorted_std_items(const std::vector<DataItem>& items) {
        return is_same(items, sorted_std_items);
    }

    static std::vector<DataItem> std_items;
    static std::vector<DataItem> sorted_std_items;
    std::shared_ptr<TrainerContext> context_ptr;
    std::unique_ptr<FileSystem> fs;
    int thread_num = 1;
    const int n_run = 5;
};

std::vector<DataItem> DataReaderOmpTest::std_items;
std::vector<DataItem> DataReaderOmpTest::sorted_std_items;

TEST_F(DataReaderOmpTest, LineDataReaderSingleThread) {
    std::unique_ptr<DataReader> data_reader(CREATE_INSTANCE(DataReader, "LineDataReader"));
    ASSERT_NE(nullptr, data_reader);

    auto config = YAML::Load(
            "parser:\n"
            "    class: LineDataParser\n"
            "pipeline_cmd: cat\n"
            "done_file: done_file\n");
    ASSERT_EQ(0, data_reader->initialize(config, context_ptr));
    auto data_file_list = data_reader->data_file_list(test_data_dir);

    const int std_items_size = std_items.size();
    ASSERT_EQ(std_items_size, data_file_list.size());

    for (int i = 0; i < std_items_size; ++i) {
        ASSERT_EQ(string::format_string("%s/%s.txt", test_data_dir, std_items[i].id.c_str()), data_file_list[i]);
    }

    int same_count = 0;
    for (int i = 0; i < n_run; ++i) {
        auto channel = framework::MakeChannel<DataItem>(128);
        ASSERT_NE(nullptr, channel);
        ASSERT_EQ(0, data_reader->read_all(test_data_dir, channel));

        std::vector<DataItem> items;
        channel->ReadAll(items);

        if (is_same_with_std_items(items)) {
            ++same_count;
        }
    }

    // n_run 次都相同
    ASSERT_EQ(n_run, same_count);
}

TEST_F(DataReaderOmpTest, LineDataReaderMuiltThread) {
    std::unique_ptr<DataReader> data_reader(CREATE_INSTANCE(DataReader, "LineDataReader"));
    ASSERT_NE(nullptr, data_reader);

    auto config = YAML::Load(
            "parser:\n"
            "    class: LineDataParser\n"
            "pipeline_cmd: cat\n"
            "done_file: done_file\n");
    ASSERT_EQ(0, data_reader->initialize(config, context_ptr));
    auto data_file_list = data_reader->data_file_list(test_data_dir);

    const int std_items_size = std_items.size();
    ASSERT_EQ(std_items_size, data_file_list.size());

    for (int i = 0; i < std_items_size; ++i) {
        ASSERT_EQ(string::format_string("%s/%s.txt", test_data_dir, std_items[i].id.c_str()), data_file_list[i]);
    }

    ASSERT_FALSE(data_reader->is_data_ready(test_data_dir));
    std::ofstream fout(fs->path_join(test_data_dir, "done_file"));
    fout << "done";
    fout.close();
    ASSERT_TRUE(data_reader->is_data_ready(test_data_dir));

    int same_count = 0;
    int sort_same_count = 0;
    for (int i = 0; i < n_run; ++i) {
        auto channel = framework::MakeChannel<DataItem>(128);
        ASSERT_NE(nullptr, channel);

        omp_set_num_threads(4);

        ASSERT_EQ(0, data_reader->read_all(test_data_dir, channel));

        std::vector<DataItem> items;
        channel->ReadAll(items);

        if (is_same_with_std_items(items)) {
            ++same_count;
        }

        std::sort(items.begin(), items.end(), [] (const DataItem& a, const DataItem& b) {
            return a.id < b.id;
        });

        if (is_same_with_sorted_std_items(items)) {
            ++sort_same_count;
        }

    }
    // n_run次有不同的（证明是多线程）
    ASSERT_EQ(4, omp_get_max_threads());
    ASSERT_GT(n_run, same_count);

    // 但排序后都是相同的
    ASSERT_EQ(n_run, sort_same_count);
}

}  // namespace feed
}  // namespace custom_trainer
}  // namespace paddle
