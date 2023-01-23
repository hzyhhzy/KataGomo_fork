mkdir data/
mkdir data/selfplay/
mkdir data/models/

source C:/ProgramData/Anaconda3/etc/profile.d/conda.sh
conda activate t

while true
do
    CUDA_VISIBLE_DEVICES="0,1" ./engine/katago selfplay -models-dir  data/models -config selfplay.cfg -output-dir data/selfplay  -max-games-total 20000 
    cd train
    bash shuffle.sh ../data ./ktmp 16 1024
    CUDA_VISIBLE_DEVICES="0" bash train.sh ..\\data b10c256nbt-fson-mish-rvgl-bnh b10c256nbt-fson-mish-rvgl-bnh 1024 main -lr-scale 1
    CUDA_VISIBLE_DEVICES="0" bash export.sh breakthrough1 ..\\data 0
    cd ..
done
