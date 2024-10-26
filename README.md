# KataHex 2024
Latest release: https://github.com/hzyhhzy/KataGo/releases/tag/Hex_20240908   
~300 elo stronger than 2022 version on 19x19 board   
New feature: Move number limitation    
## Results
The length of the optimal Hex game on n\*n board is about **0.45\*n^2** according to KataHex     
https://dev.to/hzyhhzy/analysis-of-the-length-of-optimal-games-of-hex-game-using-alphazero-like-ai-16n7   
Some conjectures of "Template problems" of Hex    
https://mathoverflow.net/questions/470376/connection-properties-of-a-single-stone-on-an-infinite-hex-board


## Training schedule
Scripts and parameters used in this run are in **./scripts**   
   
I will use **"RTX4090\*month"** or **"RTX4090\*day"** as the unit of training cost.    
Total: **~6 RTX4090\*month**       
    

The major part of this run is on 15x15 board to save money. **(~4 RTX4090\*month)**   
The cost of every selfplay game on n\*n board is **O(n^4)** (CNN cost \* move number). Consider that larger boards usually need more search visits per move, it can even reach **O(n^7)**.    
At the end it was trained on 19x19.**(~1 RTX4090\*month)**   
And finally 27x27 **(~3 RTX4090\*day)**    
    
By experience, if the largest board the model trained is n \* n, the model will play well on boards < 1.5n \* 1.5n.    
For example, for a model trained only on 15x15 and smaller boards, it plays well on 19x19 but not 27x27. After a short training on 19x19, it plays well on 27x27. At the end I trained it on 27x27, so it can barely play on a 41x41 board.
    
Later I implemented **"Move number limitation"** and continued training with this.   
On 15x15: **~4 RTX4090\*day**   
On 19x19: **~7 RTX4090\*day**   
On 27x27: **~4 RTX4090\*day** but failed. The winrate becomes weird and the reason is still unknown.