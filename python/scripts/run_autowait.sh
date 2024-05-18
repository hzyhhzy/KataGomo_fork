mkdir data/
mkdir data/selfplay/
mkdir data/models/
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/root/work/data/bins/katago-1.10/"
while true
do
    #chmod +x ./engine/katago
    #./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 25000
    cd train
    bash count_data_and_wait.sh ../data 16
    bash shuffle.sh ../data ./ktmp 32 128
    bash train.sh ../data b28c512n b28c512nbt-mish 128 main -multi-gpus 0,1
    bash export.sh renju28b ../data 0
    python view_loss.py
    cd ..
done
