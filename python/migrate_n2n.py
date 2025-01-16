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
parser.add_argument('-source', help='A smaller well-trained model which will be grafted to another bigger model, for example, b28c512nbt', required=True)
parser.add_argument('-target', help='A bigger but not well-trained or even randomly-initialized model', required=True)
parser.add_argument('-output', help='Output new checkpoint to here', required=True)
parser.add_argument('-noconvert', help='If the two tensor have different sizes, use the target model rather than convert the source', required=False, action='store_true')
args = vars(parser.parse_args())

source_path = args["source"]
target_path = args["target"]
output_path = args["output"]
noconvert = args["noconvert"]
source_data = torch.load(source_path,map_location="cpu")
target_data = torch.load(target_path,map_location="cpu")

print(source_data["config"])
print(target_data["config"])

def change_channels(target: torch.Tensor, source: torch.Tensor, name="Unknown") -> torch.Tensor:
    
    # 如果 target 和 source 的尺寸相等，则直接返回 source
    if target.shape == source.shape:
        return source
    if(noconvert):
        print(f"Warning: {name} size not match, target={target.shape}, source={source.shape}")
        return target
        
    sourceshape0=source.shape
    
    # 如果维度不一样，则直接返回 target
    if target.dim() != source.dim():
        print(f"Warning: {name} dim not match")
        raise ValueError(f"Warning: {name} dim not match")
        return target
        
    # 初始化新的张量，以source的值为基础
    # 检查所有维度是否满足条件
    if all(target.shape[dim] >= source.shape[dim] for dim in range(len(source.shape))):
        # target 的所有维度都大于等于 source
        #target=target*0.5
        slices = [slice(0, source.shape[dim]) for dim in range(len(source.shape))]
        target[tuple(slices)] = source
        return target.clone()
    elif all(target.shape[dim] <= source.shape[dim] for dim in range(len(source.shape))):
        # target 的所有维度都小于等于 source
        slices = [slice(0, target.shape[dim]) for dim in range(len(target.shape))]
        return source[tuple(slices)].clone()
    else:
        # 不满足任一条件，抛出错误
        raise ValueError("target 的所有维度必须同时大于等于或小于等于 source 的对应维度")
        return target


    # 经过所有维度检查后，source 应该和 target 的尺寸一样，做一个断言检查
    #assert target.shape == new_source.shape, "处理后的 source 形状应当和 target 一致"
    #if(target.numel()<3000):
    #    return target
    #return None



      
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
    shape_target=var_t.shape
    var_o=change_channels(var_t,var_s,varname)
    assert(shape_target==var_o.shape)
    target_data["model"][varname]=var_o
        

#print(target_data["config"],source_data["config"])

#print("blocks.26.blockstack.0.normactconv1.convpool.conv1g.weight" in target_data["model"], "blocks.26.blockstack.0.normactconv1.convpool.conv1g.weight" in source_data["model"])

print(f"Saving to {output_path}")
torch.save(target_data, output_path)

