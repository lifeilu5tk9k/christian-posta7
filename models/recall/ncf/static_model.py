# Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
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

import math
import paddle
import paddle.nn as nn
import paddle.nn.functional as F
from net import NCFLayer


class StaticModel():
    def __init__(self, config):
        self.cost = None
        self.config = config
        self._init_hyper_parameters()

    def _init_hyper_parameters(self):
        self.num_users = self.config.get("hyper_parameters.num_users")
        self.num_items = self.config.get("hyper_parameters.num_items")
        self.latent_dim = self.config.get("hyper_parameters.latent_dim")
        self.layers = self.config.get("hyper_parameters.fc_layers")
        self.learning_rate = self.config.get(
            "hyper_parameters.optimizer.learning_rate")

    def create_feeds(self, is_infer=False):
        user_input = paddle.static.data(
            name="user_input", shape=[-1, 1], dtype='int64')

        item_input = paddle.static.data(
            name="item_input", shape=[-1, 1], dtype='int64')

        if is_infer:
            return [user_input, item_input]

        label = paddle.static.data(name="label", shape=[-1, 1], dtype='int64')
        feeds_list = [user_input, item_input, label]
        return feeds_list

    def net(self, input, is_infer=False):
        ncf_model = NCFLayer(self.num_users, self.num_items, self.latent_dim,
                             self.layers)
        prediction = ncf_model(input, is_infer)

        self.inference_target_var = prediction
        if is_infer:
            fetch_dict = {'prediction': prediction}
            return fetch_dict
        cost = F.log_loss(
            input=prediction, label=paddle.cast(
                x=input[2], dtype='float32'))
        avg_cost = paddle.mean(x=cost)
        # print(avg_cost)
        self._cost = avg_cost
        fetch_dict = {'Loss': avg_cost}
        return fetch_dict

    def create_optimizer(self, strategy=None):
        optimizer = paddle.optimizer.Adam(
            learning_rate=self.learning_rate, lazy_mode=True)
        if strategy != None:
            import paddle.distributed.fleet as fleet
            optimizer = fleet.distributed_optimizer(optimizer, strategy)
        optimizer.minimize(self._cost)

    def infer_net(self, input):
        return self.net(input, is_infer=True)
