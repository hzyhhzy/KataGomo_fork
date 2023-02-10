mkdir data/
mkdir data/selfplay/
mkdir data/models/

#export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/root/lib/"  #linux

source C:/ProgramData/Anaconda3/etc/profile.d/conda.sh   #windows
conda activate t2  #windows

GPU="3"

while true
do 
    #chmod +x ./engine/katago  #linux
    CUDA_VISIBLE_DEVICES=$GPU ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 4000 
    cd train
    bash shuffle.sh ../data ./ktmp 16 128
    CUDA_VISIBLE_DEVICES=$GPU bash train.sh ../data b10c256n b10c256nbt-fson-rvgl-bnh 128 main -lr-scale 1 
    CUDA_VISIBLE_DEVICES=$GPU bash export.sh test ../data 0
    python view_loss.py
    cd ..
done
