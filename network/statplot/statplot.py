#!/bin/env python

import os
import sys
import argparse
from argparse import RawTextHelpFormatter
import math
import datetime
from datetime import timedelta

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.axes as axes

import numpy as np

figsize = (16.00, 6.00)
tsfmts = '%m-%d %H:%M'
tsfmt = mdates.DateFormatter(tsfmts)


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


def np_max_val_indx(a, ts_indx):
    m = 0
    for i in ts_indx:
        if a[i] is None:
            continue
        m1 = np.max(a[i])
        if m1 > m:
            m = m1

    return m


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


def add_count_stat(stat, dest, n, duration, multiply=100.0, accuracy=2):
    for k in stat:
        v = dest.get(k)
        if v is None:
            v = []
            dest[k] = v
        relength(v, n, 0)
        v.append(round(multiply * stat[k] / duration, accuracy))


def avg_init(stat):
    stat['min'] = []
    stat['max'] = []
    stat['avg'] = []
    stat['p99'] = []


def add_avg_stat(stat, dest, n, multiply=1.0, accurancy=2):
    if stat is None or len(stat) == 0:
        minimum = 0
        maximum = 0
        average = 0
        pcnt_95 = 0
    else:
        minimum = round(multiply * stat[0], accurancy)
        maximum = round(multiply * stat[-1], accurancy)
        average = round(multiply * percentile(stat, 0.5), accurancy)
        pcnt_95 = round(multiply * percentile(stat, 0.95), accurancy)

    dest['min'].append(minimum)
    dest['max'].append(maximum)
    dest['avg'].append(average)
    dest['p99'].append(pcnt_95)


def save_stat(fname, name, subname, date_times, stats):
    filename = "%s.%s.%s.csv" % (fname, name, subname)
    with open(filename, "w") as f:
        f.write("time\t\t")
        for k in stats:
            f.write("\t%s" % k)
        f.write("\n")
        for i in range(len(date_times)):
            f.write("%s" % date_times[i].strftime("%Y/%m/%d %H:%M:%S"))
            for k in stats:
                f.write("\t%s" % stats[k][i])
            f.write("\n")


def save_graph(fname, name, subname, date_times, stats, unit=None):
    filename = "%s.%s.%s.png" % (fname, name, subname)
    n = len(date_times)
    if n == 0:
        return

    fig, ax = plt.subplots(figsize=figsize)

    if unit is None:
        plt.title("%s %s" % (name, subname))
    else:
        plt.title("%s %s (%s)" % (name, subname, unit))

    ax.set_xlabel("Time (%s)" % tsfmts)
    ax.xaxis.set_major_formatter(tsfmt)
    plt.xticks(rotation='vertical')

    prop_cycle = plt.rcParams['axes.prop_cycle']
    colors = prop_cycle.by_key()['color']
    cc = iter(colors)
    for k in stats:
        ax.plot(date_times, stats[k], next(cc), label=k)

    plt.grid()
    ax.legend(loc='upper left', frameon=False)
    plt.savefig(filename, bbox_inches='tight', dpi=100, figsize=figsize)


#     with open(filename, "w") as f:
# f.write("time\t\t")
# for k in stats:
# f.write("\t%s" % k)
# f.write("\n")
# for i in range(len(date_times)):
# f.write("%s" % date_times[i].strftime("%Y/%m/%d %H:%M:%S"))
# for k in stats:
# f.write("\t%s" % stats[k][i])
# f.write("\n")


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
            add_count_stat(aggr_stat.connect_count, self.connect_count, n,
                           duration)
            add_avg_stat(aggr_stat.connect_time, self.connect_time, n, 0.001)
        if self.send_exist:
            add_count_stat(aggr_stat.send_count, self.send_count, n, duration)
            add_avg_stat(aggr_stat.send_time, self.send_time, n, 0.001)
            add_avg_stat(aggr_stat.send_size, self.send_size, n)
        if self.recv_exist:
            add_count_stat(aggr_stat.recv_count, self.recv_count, n, duration)
            add_avg_stat(aggr_stat.recv_time, self.recv_time, n, 0.001)
            add_avg_stat(aggr_stat.recv_size, self.recv_size, n)
        if self.msg_exist:
            add_count_stat(aggr_stat.msg_count, self.msg_count, n, duration)
            add_avg_stat(aggr_stat.msg_time, self.msg_time, n, 0.001)
            add_avg_stat(aggr_stat.msg_size, self.msg_size, n)

        self.date_times.append(aggr_stat.date_time)


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

    fname, fext = os.path.splitext(args.stat)
    if args.dir is not None:
        if os.path.exists(args.dir):
            sys.exit("%s already exist" % args.dir)
        fname = os.path.join(args.dir, os.path.basename(fname))
        os.mkdir(args.dir)

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
        save_stat(fname, "CONNECT", "RPS", c_stat.date_times,
                  c_stat.connect_count)
        save_graph(fname, "CONNECT", "RPS", c_stat.date_times,
                   c_stat.connect_count)
        for k in c_stat.connect_time:
            c_stat.connect_time[k] = np.array(c_stat.connect_time[k])
        save_stat(fname, "CONNECT", "TIME", c_stat.date_times,
                  c_stat.connect_time)
        save_graph(fname, "CONNECT", "TIME", c_stat.date_times,
                   c_stat.connect_time, "ms")

    if c_stat.send_exist:
        for k in c_stat.send_count:
            c_stat.send_count[k] = np.array(c_stat.send_count[k])
        save_stat(fname, "SEND", "RPS", c_stat.date_times, c_stat.send_count)
        save_graph(fname, "SEND", "RPS", c_stat.date_times, c_stat.send_count)
        for k in c_stat.send_time:
            c_stat.send_time[k] = np.array(c_stat.send_time[k])
        save_stat(fname, "SEND", "TIME", c_stat.date_times, c_stat.send_time)
        save_graph(fname, "SEND", "TIME", c_stat.date_times,
                   c_stat.send_time, "ms")
        for k in c_stat.send_size:
            c_stat.send_size[k] = np.array(c_stat.send_size[k])
        save_stat(fname, "SEND", "SIZE", c_stat.date_times, c_stat.send_size)

    if c_stat.recv_exist:
        for k in c_stat.recv_count:
            c_stat.recv_count[k] = np.array(c_stat.recv_count[k])
        save_stat(fname, "RECV", "RPS", c_stat.date_times, c_stat.recv_count)
        save_graph(fname, "RECV", "RPS", c_stat.date_times, c_stat.recv_count)
        for k in c_stat.recv_time:
            c_stat.recv_time[k] = np.array(c_stat.recv_time[k])
        save_stat(fname, "RECV", "TIME", c_stat.date_times, c_stat.recv_time)
        save_graph(fname, "RECV", "TIME", c_stat.date_times,
                   c_stat.recv_time, "ms")
        for k in c_stat.recv_size:
            c_stat.recv_size[k] = np.array(c_stat.recv_size[k])
        save_stat(fname, "RECV", "SIZE", c_stat.date_times, c_stat.recv_size)

    if c_stat.msg_exist:
        for k in c_stat.msg_count:
            c_stat.msg_count[k] = np.array(c_stat.msg_count[k])
        save_stat(fname, "MSG", "RPS", c_stat.date_times, c_stat.msg_count)
        save_graph(fname, "MSG", "RPS", c_stat.date_times, c_stat.msg_count)
        for k in c_stat.msg_time:
            c_stat.msg_time[k] = np.array(c_stat.msg_time[k])
        save_stat(fname, "MSG", "TIME", c_stat.date_times, c_stat.msg_time)
        save_graph(fname, "MSG", "TIME", c_stat.date_times,
                   c_stat.msg_time, "ms")
        for k in c_stat.msg_size:
            c_stat.msg_size[k] = np.array(c_stat.msg_size[k])
        save_stat(fname, "MSG", "SIZE", c_stat.date_times, c_stat.msg_size)


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
