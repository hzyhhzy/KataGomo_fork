mkdir data/
mkdir data/selfplay/
mkdir data/models/
conda activate t4
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/root/lib/"
while true
do
    chmod +x ./engine/katago
    CUDA_VISIBLE_DEVICES="0,1" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 30000
    cd train
    bash shuffle.sh ../data ./ktmp 16 96 -add-to-data-rows 0
    CUDA_VISIBLE_DEVICES="0,1" bash train.sh ../data b28c512n b28c512nbt-mish 96 trainonly -lr-scale 0.3 -multi-gpus 0,1 -samples-per-epoch 1000000
    bash shuffle.sh ../data ./ktmp 16 128 -add-to-data-rows 0
    CUDA_VISIBLE_DEVICES="0,1" bash train.sh ../data b18c384n2 b18c384nbt-mish 128 main -lr-scale 0.3 -multi-gpus 0,1 -samples-per-epoch 1000000
    
    CUDA_VISIBLE_DEVICES="0" bash export.sh hex27x3 ../data 0
    python view_loss.py
    cd ..
done
