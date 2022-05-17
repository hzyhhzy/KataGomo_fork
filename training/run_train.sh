mkdir data/
mkdir data/selfplay/
mkdir data/models/

export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/root/work/data/bins/katago-1.10/"
cd train
while true
do
    bash shuffle.sh ../data ./ktmp 2 256
    CUDA_VISIBLE_DEVICES="0" bash train.sh ../data b100c256 b100c256 256 trainonly
    CUDA_VISIBLE_DEVICES="0" bash train_lowlr.sh ../data b100c256 b100c256 256 main
    bash export.sh renju100b ../data 0
done
