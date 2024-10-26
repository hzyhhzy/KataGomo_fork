mkdir data/
mkdir data/selfplay/
mkdir data/models/
conda activate t4
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:"/home/zhq/hzy/lib/"
while true
do
    cd train
    bash shuffle.sh ../data ./ktmp 16 256
    CUDA_VISIBLE_DEVICES="1" bash train.sh ../data b18c384n_lr8 b18c384nbt-mish 256 trainonly -lr-scale 1 -multi-gpus 0 -samples-per-epoch 1200000 &
    CUDA_VISIBLE_DEVICES="2" bash train.sh ../data b18c384n_lr3 b18c384nbt-mish 256 trainonly -lr-scale 1 -multi-gpus 0 -samples-per-epoch 1200000 &
    wait
    
    CUDA_VISIBLE_DEVICES="1" bash export.sh hex15x3 ../data 0
    python view_loss.py
    cd ..
    python find_old_file.py
done
