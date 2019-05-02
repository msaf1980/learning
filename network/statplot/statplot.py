#!/bin/env python

import sys
import argparse
from argparse import RawTextHelpFormatter
import math
import datetime
from datetime import timedelta

import matplotlib.pyplot as plt
import numpy as np


def round_time(dt=None, delta=datetime.timedelta(minutes=1), to='average'):
    """
    Round a datetime object to a multiple of a timedelta
    dt : datetime.datetime object, default now.
    dateDelta : timedelta object, we round to a multiple of this, default 1 minute.
    """
    round_to = delta.total_seconds()

    if dt is None:
        dt = datetime.datetime.now()
    seconds = (dt - dt.min).seconds

    if to == 'up':
        rounding = (seconds + round_to) // round_to * round_to
    elif to == 'down':
        rounding = seconds // round_to * round_to
    else:
        rounding = (seconds + round_to / 2) // round_to * round_to

    return dt + timedelta(0, rounding - seconds, -dt.microsecond)


def percentile(N, percent, key=lambda x: x):
    """
    Find the percentile of a list of values.

    @parameter N - is a list of values. Note N MUST BE already sorted.
    @parameter percent - a float value from 0.0 to 1.0.
    @parameter key - optional key function to compute value from each element of N.

    @return - the percentile of the values
    """
    if not N:
        return None
    k = (len(N) - 1) * percent
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return key(N[int(k)])
    d0 = key(N[int(f)]) * (c - k)
    d1 = key(N[int(c)]) * (k - f)
    return d0 + d1


class Stat:
    def __init__(self, line):
        fields = line.split('\t')
        if len(fields) != 9:
            raise ValueError("incorrect fields number")

        self.timestamp = int(fields[0])
        self.testhost = fields[1]
        self.sessid = fields[2]
        self.proto = fields[3]
        self.address = fields[4]
        self.operation = fields[5]
        self.duration = int(fields[6])
        self.size = int(fields[7])
        self.status = fields[8]


def compare_stat(x, y):
    if x.timestamp == y.timestamp:
        return x.operation < y.operation
    else:
        return x.timestamp < y.timestamp


class AggrStat:
    def __init__(self, date_time, timestamp):
        self.date_time = date_time
        self.reset()
        self.start = timestamp

    def reset(self):
        self.start = 0
        self.end = 0
        self.connect_count = dict()
        self.connect_time = []
        self.send_count = dict()
        self.send_time = []
        self.send_size = []
        self.recv_count = dict()
        self.recv_time = []
        self.recv_size = []
        self.msg_count = dict()
        self.msg_time = []
        self.msg_size = []

    def increment(self, stat_count, stat):
        if stat_count.get(stat.status) is None:
            stat_count[stat.status] = 1
        else:
            stat_count[stat.status] += 1

    def add(self, stat):
        if stat.timestamp > self.end:
            self.end = stat.timestamp
        if stat.operation == 'CONNECT':
            self.increment(self.connect_count, stat)
            if stat.status == 'SUCCESS':
                self.connect_time.append(stat.duration)
        elif stat.operation == 'SEND':
            self.increment(self.send_count, stat)
            if stat.status == 'SUCCESS':
                self.send_time.append(stat.duration)
                self.send_size.append(stat.size)
        elif stat.operation == 'RECV':
            self.increment(self.recv_count, stat)
            if stat.status == 'SUCCESS':
                self.recv_time.append(stat.duration)
                self.recv_size.append(stat.size)
        elif stat.operation == 'MSG':
            self.increment(self.msg_count, stat)
            if stat.status == 'SUCCESS':
                self.msg_time.append(stat.duration)
                self.msg_size.append(stat.size)


#     def calc_rps(self, stat_count):
# trange = self.end - self.start
# for k in stat_count:
# stat_count[k] = round(100.0 * stat_count[k] / trange, 1)

    def calc_avg(self, stat_value):
        # minimum, median, pcnt_95, max
        if len(stat_value) == 0:
            return (0, 0, 0)
        minimum = stat_value[0]
        maximum = stat_value[-1]
        average = percentile(stat_value, 0.5)
        pcnt_95 = percentile(stat_value, 0.95)
        return (minimum, average, pcnt_95, maximum)

    def process_stat(self):
        self.connect_time.sort()
        self.send_time.sort()
        self.send_size.sort()
        self.recv_time.sort()
        self.recv_size.sort()
        self.msg_time.sort()
        self.msg_size.sort()
        #self.calc_rps(self.connect_count)
        #self.calc_rps(self.send_count)
        #self.calc_rps(self.recv_count)
        #self.calc_rps(self.msg_count)

    def print_stat(self):
        print(self.date_time)
        print("CONNECT", self.connect_count)
        print("time", self.calc_avg(self.connect_time))
        if len(self.send_count) > 0:
            print("SEND", self.send_count)
            print("time", self.calc_avg(self.send_time))
            print("size", self.calc_avg(self.send_size))
        if len(self.recv_count) > 0:
            print("RECV", self.recv_count)
            print("time", self.calc_avg(self.recv_time))
            print("size", self.calc_avg(self.recv_size))
        if len(self.msg_count) > 0:
            print("MSG", self.msg_count)
            print("time", self.calc_avg(self.msg_time))
            print("size", self.calc_avg(self.msg_size))


def relength(stat_count, n, default):
    while len(stat_count) < n:
        stat_count.append(default)


def add_count_stat(stat, dest, n, duration):
    for k in stat:
        v = dest.get(k)
        if v is None:
            v = []
            dest[k] = v
        relength(v, n, 0)
        v.append(round(100.0 * stat[k] / duration, 1))

def avg_init(stat):
    stat['min'] = []
    stat['max'] = []
    stat['avg'] = []
    stat['p99'] = []

def add_avg_stat(stat, dest, n):
    if stat is None or len(stat) == 0:
        minimum = 0
        maximum = 0
        average = 0
        pcnt_95 = 0
    else:
        minimum = stat[0]
        maximum = stat[-1]
        average = percentile(stat, 0.5)
        pcnt_95 = percentile(stat, 0.95)

    dest['min'].append(minimum)
    dest['max'].append(maximum)
    dest['avg'].append(average)
    dest['p99'].append(pcnt_95)


class CollectStat:
    def __init__(self, operations):
        self.connect_exist = True if "CONNECT" in operations else False
        self.send_exist = True if "SEND" in operations else False
        self.recv_exist = True if "RECV" in operations else False
        self.msg_exist = True if "MSG" in operations else False

        self.date_times = []

        if self.connect_exist:
            self.connect_count = dict()
            self.connect_time = dict()
            avg_init(self.connect_time)

        if self.send_exist:
            self.send_count = dict()
            self.send_time = dict()
            avg_init(self.send_time)
            self.send_size = dict()
            avg_init(self.send_size)

        if self.recv_exist:
            self.recv_count = dict()
            self.recv_time = dict()
            avg_init(self.recv_time)
            self.recv_size = dict()
            avg_init(self.recv_size)

        if self.msg_exist:
            self.msg_count = dict()
            self.msg_time = dict()
            avg_init(self.msg_time)
            self.msg_size = dict()
            avg_init(self.msg_size)

    def add(self, aggr_stat):
        n = len(self.date_times)
        duration = aggr_stat.end - aggr_stat.start
        if self.connect_exist:
            add_count_stat(aggr_stat.connect_count, self.connect_count, n, duration)
            add_avg_stat(aggr_stat.connect_time, self.connect_time, n)
        if self.send_exist:
            add_count_stat(aggr_stat.send_count, self.send_count, n, duration)
            add_avg_stat(aggr_stat.send_time, self.send_time, n)
            add_avg_stat(aggr_stat.send_size, self.send_size, n)
        if self.recv_exist:
            add_count_stat(aggr_stat.recv_count, self.recv_count, n, duration)
            add_avg_stat(aggr_stat.recv_time, self.recv_time, n)
            add_avg_stat(aggr_stat.recv_size, self.recv_size, n)
        if self.msg_exist:
            add_count_stat(aggr_stat.msg_count, self.msg_count, n, duration)
            add_avg_stat(aggr_stat.msg_time, self.msg_time, n)
            add_avg_stat(aggr_stat.msg_size, self.msg_size, n)

        self.date_times.append(aggr_stat.date_time)
            #self.connect_count.add(aggr_stat.connect_count)
        # print(aggr_stat.connect_count)
        #elif self.connect_exist:
        #    self.connect_count.append(0)



def parse_cmdline():
    parser = argparse.ArgumentParser(
        formatter_class=RawTextHelpFormatter,
        description='Process network benchmark log with format:\n'
        '#timestamp(ns)\ttesthost\tsession\tproto\tremote_address\toper\tduration(us)\tsize\tstatus'
    )

    parser.add_argument('-a',
                        '--aggregate',
                        dest='aggr',
                        action='store',
                        type=str,
                        default="1m",
                        help='aggregation interval (N[s m h])')

    parser.add_argument('-s',
                        '--stat',
                        dest='stat',
                        action='store',
                        type=str,
                        required=True,
                        help='stat file (sorted by timestamp)')

    parser.add_argument('-d',
                        '--dir',
                        dest='dir',
                        action='store',
                        type=str,
                        help='out dir')

    parser.add_argument('-o',
                        '--operation',
                        action='append',
                        dest='operations',
                        type=str,
                        help='processed operations [CONNECT SEND RECV MSG]\n'
                        'default CONNECT SEND RECV')

    return parser.parse_args()



def main():
    try:
        args = parse_cmdline()
        if args.aggr[-1] == 's':
            aggr_n = int(args.aggr[0:-1])
            if aggr_n != 1 and aggr_n != 10:
                raise ValueError("seconds aggreagation must be 1 or 10")
            aggr = timedelta(seconds=aggr_n)
        elif args.aggr[-1] == 'm':
            aggr_n = int(args.aggr[0:-1])
            if aggr_n != 1 and aggr_n != 10:
                raise ValueError("minutes aggreagation must be 1 or 10")
            aggr = timedelta(minutes=aggr_n)
        elif args.aggr == '1h':
            aggr_n = 1
            aggr = timedelta(hours=aggr_n)
        else:
            raise ValueError("incorrect aggregate: %s" % args.aggr)

        operations_check = {"CONNECT", "SEND", "RECV", "MSG"}
        if args.operations is not None:
            for i in range(len(args.operations)):
                args.operations[i] = args.operations[i].upper()
                if args.operations[i] not in operations_check:
                    raise ValueError("unknown operation: %s" %
                                     args.operations[i])
        else:
            args.operations = ["CONNECT", "SEND", "RECV"]

    except Exception as e:
        sys.exit(str(e))

    aggr_stat = None
    operations = set(args.operations)
    c_stat = CollectStat(operations)

    with open(args.stat, "r") as f:
        n = 0
        start = 0
        for line in f:
            if line.startswith("#"):
                continue
            line = line.rstrip()
            if line == "":
                continue
            try:
                stat = Stat(line)
                if stat.operation not in operations:
                    n += 1
                    continue
                if stat.timestamp < start:
                    sys.exit("unsorted stat file")
                elif stat.timestamp > start:
                    start = stat.timestamp

                date_time = datetime.datetime.fromtimestamp(stat.timestamp /
                                                            1000)
                round_date_time = round_time(date_time, aggr, 'down')

                if aggr_stat is None:
                    aggr_stat = AggrStat(round_date_time, stat.timestamp)
                elif aggr_stat.date_time < round_date_time:
                    aggr_stat.process_stat()
                    # aggr_stat.print_stat()

                    c_stat.add(aggr_stat)
                    # reset
                    aggr_stat = AggrStat(round_date_time, stat.timestamp)

                aggr_stat.add(stat)
            except Exception as e:
                sys.stderr.write("broken line %d: %s\n" % (n, str(e)))

            n += 1

    aggr_stat.process_stat()
    # aggr_stat.print_stat()

    c_stat.add(aggr_stat)

    c_stat.date_times = np.array(c_stat.date_times)
    if c_stat.connect_exist:
        for k in c_stat.connect_count:
            c_stat.connect_count[k] = np.array(c_stat.connect_count[k])
        for k in c_stat.connect_time:
            c_stat.connect_time[k] = np.array(c_stat.connect_time[k])

    if c_stat.send_exist:
        for k in c_stat.send_count:
            c_stat.send_count[k] = np.array(c_stat.send_count[k])
        for k in c_stat.send_time:
            c_stat.send_time[k] = np.array(c_stat.send_time[k])
        for k in c_stat.send_size:
            c_stat.send_size[k] = np.array(c_stat.send_size[k])

    if c_stat.recv_exist:
        for k in c_stat.recv_count:
            c_stat.recv_count[k] = np.array(c_stat.recv_count[k])
        for k in c_stat.recv_time:
            c_stat.recv_time[k] = np.array(c_stat.recv_time[k])
        for k in c_stat.recv_size:
            c_stat.recv_size[k] = np.array(c_stat.recv_size[k])

    if c_stat.msg_exist:
        for k in c_stat.msg_count:
            c_stat.msg_count[k] = np.array(c_stat.msg_count[k])
        for k in c_stat.msg_time:
            c_stat.msg_time[k] = np.array(c_stat.msg_time[k])
        for k in c_stat.msg_size:
            c_stat.msg_size[k] = np.array(c_stat.msg_size[k])

   # for k in send_count:
    # send_count[k] = np.array(send_count[k])
    # for k in recv_count:
    # recv_count[k] = np.array(recv_count[k])

    print(len(c_stat.date_times), c_stat.date_times)

    print("CONNECT", len(c_stat.connect_count), c_stat.connect_count)
    print("CONNECT time", c_stat.connect_time)
    print("SEND", len(c_stat.send_count), c_stat.send_count)
    print("SEND time", c_stat.send_time)
    print("RECV", len(c_stat.recv_count), c_stat.recv_count)
    print("RECV time", c_stat.recv_time)

#     print(len(connect_count), connect_count)
# print(len(send_count), send_count)
# print(len(recv_count), recv_count)

# graph_rate("connect", aggr_stat)

# aggr_stat = AggrStat(
#    round_time(datetime.datetime.fromtimestamp(stats[0].timestamp / 1000),
# aggr, 'down'))

# stats.sort(cmp=lambda x, y: compare_stat(x, y))

#     stats.sort(key=operator.attrgetter('timestamp'))
# for stat in stats:
# date_time = datetime.datetime.fromtimestamp(stat.timestamp / 1000)
# round_date_time = round_time(date_time, aggr, 'down')
# if aggr_stat.date_time == round_date_time:
# aggr_stat.add(stat)
# else:
# # calc stat

# # reset
# aggr_stat = AggrStat(round_date_time)
# aggr_stat.add(stat)

# print("[%s] [%s] %s %s %s %s %s %s %d %d %s" %
# (date_time, round_date_time, stat.timestamp, stat.testhost,
# stat.sessid, stat.proto, stat.address, stat.operation,
# stat.duration, stat.size, stat.status))

if __name__ == "__main__":
    main()
