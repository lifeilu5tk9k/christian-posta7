#   Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import paddle.fluid as fluid
import math

from paddlerec.core.utils import envs
from paddlerec.core.model import Model as ModelBase

import paddle.fluid as fluid
import paddle.fluid.layers.nn as nn
import paddle.fluid.layers.tensor as tensor
import paddle.fluid.layers.control_flow as cf

class Model(ModelBase):
    def __init__(self, config):
        ModelBase.__init__(self, config)

    def train_net(self):
        """ network definition """
       
        data = fluid.data(name="input", shape=[None, max_len], dtype='int64')
        label = fluid.data(name="label", shape=[None, 1], dtype='int64')
        seq_len = fluid.data(name="seq_len", shape=[None], dtype='int64')
        # embedding layer
        emb = fluid.embedding(input=data, size=[dict_dim, emb_dim])
        emb = fluid.layers.sequence_unpad(emb, length=seq_len)
        # convolution layer
        conv = fluid.nets.sequence_conv_pool(
            input=emb,
            num_filters=cnn_dim,
            filter_size=cnn_filter_size,
            act="tanh",
            pool_type="max")

        # full connect layer
        fc_1 = fluid.layers.fc(input=[conv], size=hid_dim)
        # softmax layer
        prediction = fluid.layers.fc(input=[fc_1], size=class_dim, act="softmax")

        cost = fluid.layers.cross_entropy(input=prediction, label=label)
        avg_cost = fluid.layers.mean(x=cost)
        acc = fluid.layers.accuracy(input=prediction, label=label) 

        self.cost = avg_cost
        self.metrics["acc"] = cos_pos

    def get_cost_op(self):
        return self.cost

    def get_metrics(self):
        return self.metrics

    def optimizer(self):
        learning_rate = 0.01
        sgd_optimizer = fluid.optimizer.Adagrad(learning_rate=learning_rate)

        return sgd_optimizer


    def infer_net(self, parameter_list):
        self.train_net()
