
source C:/ProgramData/Anaconda3/etc/profile.d/conda.sh
conda activate t

cd train
CUDA_VISIBLE_DEVICES="3" python save_model_for_export_manual.py -traindir ../data/train/b18c384n_lr3 -exportdir ../data/torchmodels_toexport -exportprefix b18c384n_lr3
CUDA_VISIBLE_DEVICES="3" python save_model_for_export_manual.py -traindir ../data/train/b18c384n_lr8 -exportdir ../data/torchmodels_toexport -exportprefix b18c384n_lr8
CUDA_VISIBLE_DEVICES="3" bash export.sh nbttest ../data 0
