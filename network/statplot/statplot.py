#!/bin/env python

import sys
import argparse
from argparse import RawTextHelpFormatter
import time
import operator
import datetime
from datetime import timedelta


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
    def __init__(self, date_time):
        self.date_time = date_time
        self.reset()

    def reset(self):
        self.connect_stat = []
        self.send_stat = []
        self.recv_stat = []
        self.msg_stat = []

    def add(self, stat):
        if stat.operation == 'CONNECT':
            self.connect_stat.append(stat)
        elif stat.operation == 'SEND':
            self.send_stat.append(stat)
        elif stat.operation == 'RECV':
            self.recv_stat.append(stat)
        elif stat.operation == 'MSG':
            self.msg_stat.append(stat)

    def print_stat(self):
        # print(self.connect_stat)
        pass


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

    parser.add_argument('-o',
                        '--out',
                        dest='out',
                        action='store',
                        type=str,
                        help='out dir')

    return parser.parse_args()


def main():
    args = parse_cmdline()
    if args.aggr[-1] == 's':
        aggr_n = int(args.aggr[0:-1])
        if aggr_n != 1 and aggr_n != 10:
            sys.exit("seconds aggreagation must be 1 or 10")
        aggr = timedelta(seconds=aggr_n)
    elif args.aggr[-1] == 'm':
        aggr_n = int(args.aggr[0:-1])
        if aggr_n != 1 and aggr_n != 10:
            sys.exit("minutes aggreagation must be 1 or 10")
        aggr = timedelta(minutes=aggr_n)
    elif args.aggr == '1h':
        aggr_n = 1
        aggr = timedelta(hours=aggr_n)
    else:
        sys.exit("incorrect aggregate: %s" % args.aggr)

    aggr_stat = None
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
                if stat.timestamp < start:
                    sys.exit("unsorted stat file")
                elif stat.timestamp > start:
                    start = stat.timestamp

                date_time = datetime.datetime.fromtimestamp(stat.timestamp /
                                                            1000)
                round_date_time = round_time(date_time, aggr, 'down')

                if aggr_stat is None:
                    aggr_stat = AggrStat(round_date_time)
                elif aggr_stat.date_time < round_date_time:
                    # calc stat
                    aggr_stat.print_stat()

                    # reset
                    aggr_stat = AggrStat(round_date_time)

                aggr_stat.add(stat)
            except Exception as e:
                sys.stderr.write("broken line %d: %s\n" % (n, str(e)))

            n += 1

    if aggr_stat is not None:
        aggr_stat.print_stat()

    #aggr_stat = AggrStat(
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
