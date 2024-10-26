import os,shutil,re,time

filelist = []
rootdir = r"data/alldata"
disdir = r"data/selfplay"

while True:

    filelist  = os.listdir(rootdir)
    min=10000000000
    choose='random'
    for i in filelist:
        j=re.findall(r'\d+', i)
        if len(j)==4:
                num=int(j[3])
                if(num<min):
                    min=num
                    choose=i
        if len(j)==5:
                num=int(j[4])
                if(num<min):
                    min=num
                    choose=i
    if 'random' in filelist:
        choose='random'
    if len(filelist)==0:
        time.sleep(300000)
        continue  #retry
    if not choose in filelist:
        time.sleep(300000)
        continue  #retry

    filepath = os.path.join( rootdir, choose )
    shutil.move(filepath,disdir)
    break


if(min<0):
    #do again

    filelist = []
    rootdir = r"data/alldata"
    disdir = r"data/selfplay"
    filelist  = os.listdir(rootdir)
    min=10000000000
    choose='random'
    for i in filelist:
        j=re.findall(r'\d+', i)
        if len(j)==4:
                num=int(j[3])
                if(num<min):
                    min=num
                    choose=i
        if len(j)==5:
                num=int(j[4])
                if(num<min):
                    min=num
                    choose=i
    if 'random' in filelist:
        choose='random'
    if not choose in filelist:
        time.sleep(1000000)

    filepath = os.path.join( rootdir, choose )
    shutil.move(filepath,disdir)
    
