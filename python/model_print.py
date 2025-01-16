#!/usr/bin/python3
import sys
import os
import argparse
import math
import torch

description = """
replace all variable of target model whose name is in the source model
"""

parser = argparse.ArgumentParser(description=description)
parser.add_argument('model', help='model')
args = vars(parser.parse_args())

model_path = args["model"]
data = torch.load(model_path,map_location="cpu")

print(data["config"])
print(data.keys())
print(data["train_state"])
print(data["swa_model"].keys())

