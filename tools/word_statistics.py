#!/usr/bin/env python3

import argparse
import os
import re
from collections import Counter


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        "--csv_file ", dest="csv_file", help="model file", required=True
    )
    parser.add_argument(
        "--print_count ",
        dest="print_count",
        help="print the first number few words, default 50",
        default=50,
    )
    args = parser.parse_args()

    csv_file = args.csv_file
    print_count = int(args.print_count)
    print(f"handle csv_file: {csv_file}, print_count: {print_count}")
    assert os.path.isfile(csv_file), f"csv_file: {csv_file} is not a file"

    all_words = []
    skip_list = [
        "",
        ",",
        "pc",
        "bool",
        '"\n',
        '"#00',
        "const&",
        ">",
        "void*>",
        "long",
        "unsigned",
        "8ul>::type",
        "/apex/com.android.runtime/lib64/bionic/libc.so",
        "/apex/com.android.runtime/lib/bionic/libc.so",
        "const",
        "int",
        "4u>",
        "true>:",
    ]
    with open(csv_file, "r") as f:
        lines = f.readlines()
        for line in lines:
            data = line.split(",")
            for i in data:
                for j in i.split(" "):
                    if j.isdigit() or j in skip_list:
                        continue
                    if len(j) > 1 and j[1:].isdigit():
                        continue
                    all_words.append(j)

    # print(all_words)
    c_data = Counter(all_words)
    sorted_words = sorted(c_data.items(), key=lambda x: x[1], reverse=True)
    cut_words = sorted_words
    if len(sorted_words) > print_count:
        cut_words = sorted_words[:print_count]

    # print(cut_words)
    for i in cut_words:
        print("key: {}: count: {}".format(i[0].replace("\n", ""), i[1]))


if __name__ == "__main__":
    main()
