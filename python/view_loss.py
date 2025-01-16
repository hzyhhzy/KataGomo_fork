baseDir="../data/train/"
#lossItems={"p0loss":(2.2,2.5),"vloss":(0.65,0.8),"loss":(0,0),"gnorm_batch":(0,1e4),"norm_normal_batch":(0,0),"norm_normal_gamma_batch":(0,0),"norm_output_batch":(0,0)}#name,ylim,  0 means default
lossItems={"Ip0loss":(2.2,2.55),"Ivloss":(0.65,0.8),"loss":(51,55),"Ipacc1":(0.30,0.43),"gnorm_batch":(0,1.5e4),"exgnorm":(0,3e3),"norm_normal_batch":(3000,20000)}#name,ylim,  0 means default
#trainDirs=["b9c192nbt1","b8c128nbt","b4c192nbt1","b25c53","b17c64","b7c100","b8c256nbt1","b12c224nb1t","b12c224lbt"] 
#trainDirs=["b12c240nb1t","b12c240nb1t_adam","b12c240nb1t_adam2","b12c240nb1t_adam3","b12c240nb1t_sgd_lr8","b12c240nb1t_adam4","b12c240nb1t_adam5","b12c240nb1t_adam_lr2","b12c240nb1t_adam6","b12c240nb1t_adam_lr2_fastdrop","b12c240nb1t_adam6_fastdrop","b18c384n"]
#trainDirs=["b6c64sgd3","b6c64sgd3fd1","b6c64sgd3fd2","b6c64sgd3fd3","b6c64sgd","b6c64sgd2","b6c64a","b6c64"]
trainDirs=["b6c64sgd8","b12c64lr2","tf6b"]
#trainDirs=["b6c64sgd8_wdpow05",]
#trainDirs=["b6c64sgd3a","b6c64sgd6","b6c64sgd7","b6c64sgd3afd7","b6c64sgd3afd6","b6c64sgd3afd5","b6c64sgd3afd4","b6c64sgd3fd1","b6c64sgd3fd2","b6c64sgd3","b6c64sgd3fd3"]
autoBias=False
biases=[0,]
scales=[1,]
#lossTypes=["train","val"]
lossTypes=["train"]
outputFile="../loss.png"

logPlot=True
logPlotXmin=1e7
#logPlotXmax=3e9
logPlotXmax=None
smooth_window=10


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
        if("p0loss" not in j):
            continue
        if("nsamp_train" in j):
            nsamp=j["nsamp_train"]
        else:
            nsamp = j["nsamp"]

        d["nsamp"].append(nsamp)
        for key in lossKeys:
            d[key].append(j[key])
    return d



#os.makedirs(outputDir,exist_ok=True)


fig=plt.figure(figsize=(12,8*nKeys),dpi=100)
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
        y_major_locator=None
        y_minor_locator=None
        if(plotLim[1]-plotLim[0]>5):
            pass
        elif(plotLim[1]-plotLim[0]>2):
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
        elif(plotLim[1]-plotLim[0]>0.01):
            y_major_locator=MultipleLocator(0.005)
            y_minor_locator=MultipleLocator(0.001)
        if(y_major_locator is not None):
            ax.yaxis.set_major_locator(y_major_locator)
            ax.yaxis.set_minor_locator(y_minor_locator)
        
maxX=0
isSingleDir = len(trainDirs)==1
if(isSingleDir):
    fig.suptitle(trainDirs[0])
for trainDirId in range(len(trainDirs)):
    trainDir=trainDirs[trainDirId]
    b=0
    s=1
    if biases is not None and len(biases)>trainDirId:
        b=biases[trainDirId]
    if scales is not None and len(scales)>trainDirId:
        s=scales[trainDirId]
    for lossType in lossTypes:
        jsonPath=os.path.join(baseDir,trainDir,"metrics_"+lossType+".json")
        if(not os.path.exists(jsonPath)):
            print("warning:",jsonPath,"not exists")
            continue
        jsonData=readJsonFile(jsonPath,lossKeys)

        for i in range(nKeys):
            key = lossKeys[i]
            ax = plt.subplot(nKeys, 1, i + 1)
            plotLabel=lossType if isSingleDir else trainDir+"."+lossType
            xdata=jsonData["nsamp"]
            xdata=[s*x+b for x in xdata]
            #if(trainDir=="b12c64n2n"):
            #    xdata=[x+7e9 for x in xdata]
            if(trainDirId==0):
                maxX=max(xdata)
            if(autoBias):
                b1=maxX-max(xdata)
                xdata=[x+b1 for x in xdata]
            from scipy.signal import savgol_filter
            ydata=jsonData[key]
            if smooth_window>0:
                ydata = savgol_filter(ydata, window_length=smooth_window, polyorder=1)  # Adjust window_length and polyorder as needed
    
            #ax.plot(xdata, ydata,'.', label=plotLabel)
            ax.scatter(xdata, ydata, label=plotLabel, s=1, marker='o')
            ax.legend(loc="upper right")
            if logPlot:
                plt.xscale('log')
                ax.set_xlim(logPlotXmin,logPlotXmax)


plt.tight_layout()
plt.savefig(outputFile)