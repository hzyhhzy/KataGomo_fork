mkdir data/
mkdir data/selfplay/
mkdir data/models/
conda activate t4
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/root/lib/"
while true
do
    chmod +x ./engine/katago
    CUDA_VISIBLE_DEVICES="0,1" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 40000
    #CUDA_VISIBLE_DEVICES="0,1" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 60000
    cd train
    bash shuffle.sh ../data ./ktmp 16 256
    CUDA_VISIBLE_DEVICES="0,1" bash train.sh ../data b18c384n3 b18c384nbt-mish 256 main -multi-gpus 0,1 -samples-per-epoch 1000000
    #CUDA_VISIBLE_DEVICES="0,1" bash train.sh ../data b10c384n b10c384nbt-mish 256 main -lr-scale 1 -multi-gpus 0,1 -samples-per-epoch 1000000
    
    CUDA_VISIBLE_DEVICES="0" bash export.sh hexmm15x ../data 0
    python view_loss.py
    cd ..
done
