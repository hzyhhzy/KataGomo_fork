mkdir data/
mkdir data/selfplay/
mkdir data/models/
conda activate t4
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/home/zhq/hzy/lib/"
while true
do
    chmod +x ./engine/katago
    CUDA_VISIBLE_DEVICES="1,2" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 50000
    CUDA_VISIBLE_DEVICES="1,2" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 50000
    cd train
    bash shuffle.sh ../data C:/ktmp 16 256 -summary-file ../selfplay_summary.json
    CUDA_VISIBLE_DEVICES="1" bash train.sh ../data b18c384n2 b18c384nbt-mish 256 main -lr-scale 2 -multi-gpus 0 -samples-per-epoch 1200000
    
    CUDA_VISIBLE_DEVICES="0" bash export.sh hex15x3 ../data 0
    python view_loss.py
    cd ..
done
