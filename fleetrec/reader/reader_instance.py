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
from __future__ import print_function
import sys

from fleetrec.utils.envs import lazy_instance

if len(sys.argv) != 3:
    raise ValueError("reader only accept two argument: initialized reader class name and TRAIN/EVALUATE")

reader_package = sys.argv[1]

if sys.argv[2] == "TRAIN":
    reader_name = "TrainReader"
else:
    reader_name = "EvaluateReader"

reader_class = lazy_instance(reader_package, reader_name)
reader = reader_class()
reader.run_from_stdin()
