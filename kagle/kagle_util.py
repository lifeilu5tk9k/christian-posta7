import os
import sys
import time
import datetime
import kagle_fs
import numpy as np
from paddle.fluid.incubate.fleet.parameter_server.pslib import fleet

def get_env_value(env_name):
    return os.popen("echo -n ${" + env_name + "}").read().strip()

def now_time_str():
    return "\n" + time.strftime("%Y-%m-%d %H:%M:%S",time.localtime()) + "[0]:"

def get_absolute_path(path, params):
    if path.startswith('afs:') or path.startswith('hdfs:'):
        sub_path = path.split('fs:')[1]
        if ':' in sub_path: #such as afs://xxx:prot/xxxx
            return path
        elif 'fs_name' in params:
            return params['fs_name'] + sub_path
    else:
        return path

def make_datetime(date_str, fmt = None):
    if fmt is None:
        if len(date_str) == 8: #%Y%m%d
            return datetime.datetime.strptime(date_str, '%Y%m%d')
        if len(date_str) == 12: #%Y%m%d%H%M
            return datetime.datetime.strptime(date_str, '%Y%m%d%H%M')
    return datetime.datetime.strptime(date_str, fmt)


def wroker_numric_opt(value, opt):
    local_value = np.array([value])
    global_value = np.copy(local_value) * 0
    fleet._role_maker._node_type_comm.Allreduce(local_value, global_value, op=opt)
    return global_value[0]

def worker_numric_sum(value):
    from mpi4py import MPI
    return wroker_numric_opt(value, MPI.SUM)
def worker_numric_avg(value):
    return worker_numric_sum(value) / fleet.worker_num()
def worker_numric_min(value):
    from mpi4py import MPI
    return wroker_numric_opt(value, MPI.MIN)
def worker_numric_max(value):
    from mpi4py import MPI
    return wroker_numric_opt(value, MPI.MAX)
    

def rank0_print(log_str):
    print_log(log_str, {'master': True})

def print_log(log_str, params):
    if params['master']:
        if fleet.worker_index() == 0:
            print(log_str)
            sys.stdout.flush()
    else:
        print(log_str)
    if 'stdout' in params:
        params['stdout'] += str(datetime.datetime.now()) + log_str
             
def print_cost(cost, params):
    log_str = params['log_format'] % cost
    print_log(log_str, params) 
    return log_str
        

class CostPrinter:
    def __init__(self, callback, callback_params):
        self.reset(callback, callback_params)
        pass
        
    def __del__(self):
        if not self._done:
            self.done()
        pass
        
    def reset(self, callback, callback_params):
        self._done = False
        self._callback = callback
        self._callback_params = callback_params
        self._begin_time = time.time()
        pass
        
    def done(self):
        cost = time.time() - self._begin_time
        log_str = self._callback(cost, self._callback_params) #cost(s)
        self._done = True
        return cost, log_str

class PathGenerator:
    def __init__(self, config):
	self._templates = {}
        self.add_path_template(config)
        pass
    
    def add_path_template(self, config):
        if 'templates' in config:
            for template in config['templates']:
                self._templates[template['name']] = template['template']
        pass

    def generate_path(self, template_name, param):
        if template_name in self._templates:
            if 'time_format' in param:
                str = param['time_format'].strftime(self._templates[template_name])
                return str.format(**param)
            return self._templates[template_name].format(**param)
        else:
            return ""

class TimeTrainPass:
    def __init__(self, global_config):
        self._config = global_config['epoch']
        if '+' in self._config['days']:
            day_str = self._config['days'].replace(' ', '')
            day_fields = day_str.split('+')
            self._begin_day = make_datetime(day_fields[0].strip())
            if len(day_fields) == 1 or len(day_fields[1]) == 0:
                #100 years, meaning to continuous running
                self._end_day = self._begin_day + datetime.timedelta(days=36500) 
            else:                     
                # example: 2020212+10 
                run_day = int(day_fields[1].strip())
                self._end_day =self._begin_day + datetime.timedelta(days=run_day)
        else: 
            # example: {20191001..20191031}
            days = os.popen("echo -n " + self._config['days']).read().split(" ")
            self._begin_day = make_datetime(days[0])
            self._end_day = make_datetime(days[len(days) - 1])
        self._checkpoint_interval = self._config['checkpoint_interval']
        self._dump_inference_interval = self._config['dump_inference_interval']
        self._interval_per_pass = self._config['train_time_interval'] #train N min data per pass

        self._pass_id = 0
        self._inference_pass_id = 0
        self._pass_donefile_handler = None
        if 'pass_donefile_name' in self._config:
            self._train_pass_donefile = global_config['output_path'] + '/' + self._config['pass_donefile_name']
            if kagle_fs.is_afs_path(self._train_pass_donefile):
                self._pass_donefile_handler = kagle_fs.FileHandler(global_config['io']['afs'])
            else:
                self._pass_donefile_handler = kagle_fs.FileHandler(global_config['io']['local_fs'])
            
            last_done = self._pass_donefile_handler.cat(self._train_pass_donefile).strip().split('\n')[-1]
            done_fileds = last_done.split('\t')
            if len(done_fileds) > 4:
                self._base_key = done_fileds[1]
                self._checkpoint_model_path = done_fileds[2]
                self._checkpoint_pass_id = int(done_fileds[3])
                self._inference_pass_id =  int(done_fileds[4])
                self.init_pass_by_id(done_fileds[0], self._checkpoint_pass_id)

    def max_pass_num_day(self):
        return 24 * 60 / self._interval_per_pass
    
    def save_train_progress(self, day, pass_id, base_key, model_path, is_checkpoint):
        if is_checkpoint:
            self._checkpoint_pass_id = pass_id
            self._checkpoint_model_path = model_path
        done_content  = "%s\t%s\t%s\t%s\t%d\n" % (day, base_key, 
            self._checkpoint_model_path, self._checkpoint_pass_id, pass_id)
        self._pass_donefile_handler.write(done_content, self._train_pass_donefile, 'a')
        pass

    def init_pass_by_id(self, date_str, pass_id):
        date_time = make_datetime(date_str) 
        if pass_id < 1:
            pass_id = 0
        if (date_time - self._begin_day).total_seconds() > 0:
            self._begin_day = date_time
        self._pass_id = pass_id
        mins = self._interval_per_pass * (pass_id - 1)
        self._current_train_time = date_time + datetime.timedelta(minutes=mins)
        print(self._current_train_time)
    
    def init_pass_by_time(self, datetime_str):
        self._current_train_time = make_datetime(datetime_str)
        minus = self._current_train_time.hour * 60 + self._current_train_time.minute;
        self._pass_id = minus / self._interval_per_pass + 1

    def current_pass():
        return self._pass_id
        
    def next(self):
        has_next = True
        old_pass_id = self._pass_id
        if self._pass_id < 1:
            self.init_pass_by_time(self._begin_day.strftime("%Y%m%d%H%M"))
        else:
            next_time = self._current_train_time + datetime.timedelta(minutes=self._interval_per_pass)
            if (next_time - self._end_day).total_seconds() > 0:
                has_next = False
            else:
                self.init_pass_by_time(next_time.strftime("%Y%m%d%H%M"))
        if has_next and (self._inference_pass_id < self._pass_id or self._pass_id < old_pass_id):
            self._inference_pass_id = self._pass_id - 1
        return has_next

    def is_checkpoint_pass(self, pass_id):
        if pass_id < 1:
            return True
        if pass_id == self.max_pass_num_day():
            return False
        if pass_id % self._checkpoint_interval == 0:
            return True
        return False
    
    def need_dump_inference(self, pass_id):
        return self._inference_pass_id < pass_id and pass_id % self._dump_inference_interval == 0

    def date(self, delta_day=0):
        return (self._current_train_time + datetime.timedelta(days=delta_day)).strftime("%Y%m%d")

    def timestamp(self, delta_day=0):
        return (self._current_train_time + datetime.timedelta(days=delta_day)).timestamp()
