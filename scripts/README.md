# This folder include training scripts which were actually used in the latest run

**The parameters in these scripts is what I used at the END of the training. You should probably change -lr-scale before using these scripts**   
-lr-scale is in run_train.sh and train/train.sh. The parameters in run_train.sh will override train/train.sh  
-lr-scale 2.0 is good for beginning

## hex15x3early
The early stage of hex15x3. At the end retrained a model with higher learning rate (-lr-scale 2.0)

## hex15x3
"3" in "hex15x3" means this is 3rd run. There were two test runs before this.   
The major part of this run is on 15x15 board to save money. **(~4 RTX4090\*month)**    
I also trained a b28c512nbt, but not much stronger (about +30elo), so not released. 

## hex19x3
At the latter of the training it was trained on 19x19.**(~1 RTX4090\*month)**    

## hex27x3
Finally on 27x27 **(~3 RTX4090\*day)**    
    
## hexmm: Training with "Move number limitation"
"mm" means "max move"   

On 15x15 **(hexmm15x)** : **~4 RTX4090\*day**   
On 19x19 **(hexmm19x)** : **~7 RTX4090\*day**   
On 27x27 **(hexmm27x)** : **~4 RTX4090\*day** but failed. The winrate becomes weird and the reason is still unknown. Maybe because I changed"-swa-scale to 0.3 but -swa-scale should never be < 1? (Disabled swa is 1.0, not 0.0)
