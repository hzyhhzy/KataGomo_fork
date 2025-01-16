#!/usr/bin/python3
import sys
import os
import argparse
import math
import torch
import numpy as np

description = """
Print the square mean of every layer
"""

parser = argparse.ArgumentParser(description=description)
parser.add_argument('-model', default="../data/train/b40graft1/checkpoint.ckpt", required=False)
args = vars(parser.parse_args())

data_path = args["model"]
data = torch.load(data_path,map_location="cpu")

print(data["config"])
      

#print(data["model"].keys())
for varname in data["model"].keys():
    param=data["model"][varname]
    size=param.numel()
    if(size<10000):
        continue
    shape=param.shape
    l2mean = math.sqrt(torch.mean(param*param))
    print(f"{varname} shape={shape} L2mean={l2mean}")
