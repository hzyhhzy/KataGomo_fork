#!/usr/bin/python3
import sys
import os
import argparse
import math
import torch

description = """
half all channels in the source model 
"""


parser = argparse.ArgumentParser(description=description)
parser.add_argument('-source', help='A well-trained bigger model',default="../models/b28c512nbt-s776/model.ckpt")
parser.add_argument('-target', help='A model with exactly half channels of the bigger but not well-trained model',default="../data/train/b28c504n/checkpoint.ckpt")
parser.add_argument('-output', help='Output new checkpoint to here',default="../data/train/b28c504nst/checkpoint.ckpt")
args = vars(parser.parse_args())

source_path = args["source"]
target_path = args["target"]
output_path = args["output"]
source_data = torch.load(source_path,map_location="cpu")
target_data = torch.load(target_path,map_location="cpu")

#print(source_data["config"])

def half_tensor_channels(target: torch.Tensor, source: torch.Tensor, name="Unknown") -> torch.Tensor:
    # 如果 target 和 source 的尺寸相等，则直接返回 source
    if target.shape == source.shape:
        return source
    sourceshape0=source.shape
    
    # 如果维度不一样，则直接返回 target
    if target.dim() != source.dim():
        print(f"Warning: {name} dim not match")
        return target
    
    # 对每个维度进行检查
    strink_times=0
    for dim in range(target.dim()):
        if target.size(dim) < source.size(dim):
            # 如果 target 的这个维度小于 source ，则删除source最后几个channel
            strink_times+=1
            rmd=source.size(dim)-target.size(dim)
            t=source.narrow(dim, target.size(dim), rmd).contiguous()
            source = source.narrow(dim, 0, target.size(dim)).contiguous()
            source.narrow(dim, 0, rmd).add_(t).div_(2)
        elif target.size(dim) != source.size(dim):
            print(f"Warning: {name} size not same or smaller, source={sourceshape0}, target={target.shape}")
            # 如果某个维度 target 既不等于 source 也不是 source 的一半，直接返回 target
            return target
        


    # 经过所有维度检查后，source 应该和 target 的尺寸一样，做一个断言检查
    assert target.shape == source.shape, "处理后的 source 形状应当和 target 一致"
    #if(target.numel()<3000):
    #    return target
    return source



      
if "optimizer" in target_data:
    print("Deleting optimizer state")
    del target_data["optimizer"]
if "swa_model" in target_data:
    print("Deleting swa model state")
    del target_data["swa_model"]

#print(source_data["model"].keys(),target_data["model"].keys())
for varname in source_data["model"].keys():
    if(len(varname)>7 and varname[:7]=="module."):
        varname=varname[7:]
    varname1="module."+varname
    if varname not in target_data["model"] and varname1 not in target_data["model"]:
        print(f"{varname} is in source but not in target model")

#print(target_data["model"].keys())
for varname in target_data["model"].keys():

    varname1 = "module."+varname
    if(len(varname)>7 and varname[:7]=="module."):
        varname1=varname[7:]

    var_t=target_data["model"][varname]

    if varname in source_data["model"]:
        var_s=source_data["model"][varname]
    elif varname1 in source_data["model"]:
        var_s=source_data["model"][varname1]
    else:
        print(f"Variable {varname} is not in source model")
        continue
    var_halved=half_tensor_channels(var_t,var_s,varname)
    target_data["model"][varname]=var_halved
        

#print(target_data["config"],source_data["config"])

#print("blocks.26.blockstack.0.normactconv1.convpool.conv1g.weight" in target_data["model"], "blocks.26.blockstack.0.normactconv1.convpool.conv1g.weight" in source_data["model"])

print(f"Saving to {output_path}")
torch.save(target_data, output_path)
