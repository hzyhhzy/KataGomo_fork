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
parser.add_argument('-source', help='A well-trained model which will be grapted to another model', required=True)
parser.add_argument('-target', help='A not well-trained model', required=True)
parser.add_argument('-output', help='Output new checkpoint to here', required=True)
parser.add_argument('-blockbias', type=int, help='Copy source\'s n+blockbias th block to target\'s n th block', required=False, default=0)
args = vars(parser.parse_args())

source_path = args["source"]
target_path = args["target"]
output_path = args["output"]
bb = args["blockbias"]
source_data = torch.load(source_path,map_location="cpu")
target_data = torch.load(target_path,map_location="cpu")

#print(source_data["config"])
      
if "optimizer" in target_data:
    print("Deleting optimizer state")
    del target_data["optimizer"]
if "swa_model" in target_data:
    print("Deleting swa model state")
    del target_data["swa_model"]


import re
def replace_block_number(string, n):
    # 使用正则表达式匹配 "block.<数字>.<任意内容>" 这种格式
    match = re.search(r"blocks\.(\d+)\..*", string)
    if match:
        #print(string)
        # 提取原始数字并增加 n
        original_number = int(match.group(1))
        new_number = original_number + n
        # 替换字符串中的数字部分
        new_string = re.sub(r"blocks\.(\d+)\.(.*)", f"blocks.{new_number}.\\2", string)
        return new_string
    else:
        return string  # 如果不匹配，返回原字符串


for varname in source_data["model"].keys():
    v=varname
    if(len(varname)>7 and varname[:7]=="module."):
        varname=varname[7:]
    varname=replace_block_number(varname,-bb)
    varname1="module."+varname
    if varname not in target_data["model"] and varname1 not in target_data["model"]:
        print(f"{v} is in source but {varname} not in target model")


#print(target_data["model"].keys())
for varname in target_data["model"].keys():
    varname1=varname
    if(len(varname)>7 and varname[:7]=="module."):
        varname1=varname[7:]
    varname1=replace_block_number(varname1,bb)

    var_t=target_data["model"][varname]

    if "module."+varname1 in source_data["model"]:
        varname1="module."+varname1
    elif varname1 in source_data["model"]:
        pass
    else:
        print(f"Variable {varname1} is not in source model but {varname} is in target model")
        continue
        
    var_s=source_data["model"][varname1]
        
    shape1=target_data["model"][varname].shape
    shape2=source_data["model"][varname1].shape
    if(shape1==shape2):
        print(f"Copy {varname1} shape={shape1} from source to target model {varname}")
        target_data["model"][varname]=source_data["model"][varname1]
    else:
        print(f"Size of {varname} not match. source={shape2}, target={shape1}")
        



print(f"Saving to {output_path}")
torch.save(target_data, output_path)
