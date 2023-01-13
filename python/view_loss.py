baseDir="../data/train/"
lossItems={"p0loss":(1.5,2.5),"vloss":(0.5,0.9),"loss":(0,0)} #name,ylim,  0 means default
trainDirs=["b10c256nbt-fson-mish-rvgl-bnh"];
lossTypes=["train","val"]
outputFile="../loss.png"



lossKeys=list(lossItems.keys())
nKeys=len(lossKeys)



import json
import os
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator

def readJsonFile(path,lossKeys):
    d={}
    d["nsamp"]=[]
    for key in lossKeys:
        d[key]=[]

    f=open(path,"r")
    filelines=f.readlines()
    for line in filelines:
        if(len(line)<5):
            continue #bad line
        j=json.loads(line)
        if("nsamp_train" in j):
            nsamp=j["nsamp_train"]
        else:
            nsamp = j["nsamp"]

        d["nsamp"].append(nsamp)
        for key in lossKeys:
            d[key].append(j[key])
    return d



#os.makedirs(outputDir,exist_ok=True)


fig=plt.figure(figsize=(6,4*nKeys),dpi=400)
plt.subplots_adjust(hspace=0.5)
for i in range(nKeys):
    key=lossKeys[i]
    ax=plt.subplot(nKeys,1,i+1)

    plotLim=lossItems[key]
    ax.set_xlabel("nsamp")
    ax.set_ylabel(key)
    ax.set_title(key)

    if(plotLim[0]!=0 or plotLim[1]!=0):
        ax.set_ylim(plotLim[0],plotLim[1])
        if(plotLim[1]-plotLim[0]>2):
            y_major_locator=MultipleLocator(0.5)
            y_minor_locator=MultipleLocator(0.1)
        elif(plotLim[1]-plotLim[0]>0.5):
            y_major_locator=MultipleLocator(0.1)
            y_minor_locator=MultipleLocator(0.02)
        elif(plotLim[1]-plotLim[0]>0.25):
            y_major_locator=MultipleLocator(0.05)
            y_minor_locator=MultipleLocator(0.01)
        elif(plotLim[1]-plotLim[0]>0.10):
            y_major_locator=MultipleLocator(0.02)
            y_minor_locator=MultipleLocator(0.01)
        elif(plotLim[1]-plotLim[0]>0.02):
            y_major_locator=MultipleLocator(0.01)
            y_minor_locator=MultipleLocator(0.002)
        else:
            y_major_locator=MultipleLocator(0.005)
            y_minor_locator=MultipleLocator(0.001)
        ax.yaxis.set_major_locator(y_major_locator)
        ax.yaxis.set_minor_locator(y_minor_locator)

isSingleDir = len(trainDirs)==1
if(isSingleDir):
    fig.suptitle(trainDirs[0])
for trainDir in trainDirs:
    for lossType in lossTypes:
        jsonPath=os.path.join(baseDir,trainDir,"metrics_"+lossType+".json")
        jsonData=readJsonFile(jsonPath,lossKeys)

        for i in range(nKeys):
            key = lossKeys[i]
            ax = plt.subplot(nKeys, 1, i + 1)
            plotLabel=lossType if isSingleDir else trainDir+"."+lossType
            ax.plot(jsonData["nsamp"], jsonData[key], label=plotLabel)
            ax.legend(loc="upper right")


plt.savefig(outputFile)