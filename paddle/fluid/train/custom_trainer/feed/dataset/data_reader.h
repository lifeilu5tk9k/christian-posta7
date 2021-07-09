/* DataReader
 * 对指定数据的读取
 */
#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <yaml-cpp/yaml.h>
#include "paddle/fluid/framework/channel.h"

namespace paddle {
namespace custom_trainer {
namespace feed {

class TrainerContext;

struct FeatureItem {
    uint64_t feature_sign;
    uint16_t slot_id;
};

struct SampleInstance {
    std::string id;
    std::vector<float> lables;
    std::vector<FeatureItem> features;
    std::vector<float> embedx;
};

class DataItem {
public:
    DataItem() {}
    virtual ~DataItem() {}
    std::string id;  //样本id标识，可用于shuffle
    std::string data;//样本数据， maybe压缩格式
};

class DataParser {
public:
    DataParser() {}
    virtual ~DataParser() {}
    virtual int initialize(const YAML::Node& config, std::shared_ptr<TrainerContext> context) = 0;
    virtual int parse(const std::string& str, DataItem& data) const {
        return parse(str.c_str(), str.size(), data);
    }
    virtual int parse(const char* str, size_t len, DataItem& data) const = 0;
    virtual int parse_to_sample(const DataItem& data, SampleInstance& instance) const = 0;  
};
REGISTER_REGISTERER(DataParser);

class DataReader {
public:
    DataReader() {}
    virtual ~DataReader() {}
    virtual int initialize(const YAML::Node& config, std::shared_ptr<TrainerContext> context) = 0;
    //判断样本数据是否已就绪，就绪表明可以开始download
    virtual bool is_data_ready(const std::string& data_dir) = 0;
    //读取数据样本流中
    virtual int read_all(const std::string& data_dir, ::paddle::framework::Channel<DataItem> data_channel) = 0;
    virtual const DataParser* get_parser() {
        return _parser.get();
    }
private:
    std::shared_ptr<DataParser> _parser;
    std::string _data_converter_shell;
};
REGISTER_REGISTERER(DataReader);


//TODO
//HDFS/DISK Reader

}//namespace feed
}//namespace custom_trainer
}//namespace paddle
